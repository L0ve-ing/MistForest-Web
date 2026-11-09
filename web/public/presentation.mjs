// Presentation consumes only the current player's committed, filtered views.
// Sprite registration, timing and effect rows come from the native assets.
const clamp=(value,min=0,max=1)=>Math.max(min,Math.min(max,value));
const ease=value=>{const x=clamp(value);return x*x*(3-2*x);};
const mod=(value,length)=>(value%length+length)%length;
const rowCuts=[null,null,null,
  [0,208,418,617,840,1060,1299,1511,1774],
  [0,222,457,672,889,1107,1328,1552,1774],
  [0,225,450,693,927,1129,1330,1551,1774],
  [0,263,503,739,935,1143,1345,1574,1774]];
const styleAssets={1:'Cross',2:'Snipe',3:'Bite',4:'Shotgun',5:'ShadowCut',7:'Arrow',8:'Claw',
  9:'CrimsonClaw',11:'Fire',12:'RosePlant',13:'RoseBloom',14:'SwordStab',15:'SwordThrow',
  16:'SwordRecall',19:'ShadowLink',20:'Blood',21:'ArmorUp',22:'ArmorDown',23:'Silence',
  25:'Build',26:'Demolish',27:'FireStep',28:'SwordLanding',29:'Burn'};
const assetAliases={KnightHunt:'KnightCharge',Advance:'SwordLanding',Wither:'RoseWither',
  Petals:'RosePetals',TransformWolf:'WolfForm',TransformHuman:'HumanForm',KnightLeap:'Leap',
  KnightRetreat:'Retreat',HeavyPrivateAdvance:'HeavyDash',Shield:'ShieldHit',Health:'HealthHit',Impact:'PublicHit'};
function effectAsset(effect){
  if(effect.style===6)return effect.role===4?'HeavyCharge':'KnightCharge';
  if(effect.style===10)return effect.role===10?'GuardThrust':'ArmigerThrust';
  if(effect.style===17)return effect.role===4?'HeavyDash':effect.role===3?'ShadowDash':'InkDash';
  if(effect.style===18)return effect.form?'WolfForm':'HumanForm';
  if(effect.style===24)return effect.kind===14?'Retreat':'Leap';
  return styleAssets[effect.style];
}
function frameAt(clip,elapsed){
  const durations=clip.durations_ms,total=durations.reduce((sum,ms)=>sum+ms,0);
  let time=mod(elapsed,total);
  for(let frame=0;frame<durations.length;frame++){if(time<durations[frame])return frame;time-=durations[frame];}
  return 0;
}

export class Presentation {
  constructor(onChange){
    this.onChange=onChange;this.actors=new Map();this.effects=new Map();this.images=new Map();
    this.frames=new Map();this.nodes=[];this.queue=[];this.active=null;this.cells=new Map();
    this.full=localStorage.getItem('mistforest-animation')!=='simple'&&!matchMedia('(prefers-reduced-motion: reduce)').matches;
    this.speed=Number(localStorage.getItem('mistforest-animation-speed'))||1;
    this.epoch=-1;this.seen=-1;this.scene='';this.failed=false;this.lastTick=0;this.dirty=true;
    this.ready=this.load();
    document.addEventListener('visibilitychange',()=>{
      if(document.hidden){cancelAnimationFrame(this.raf);this.raf=null;this.skip();}
      else this.schedule();
    });
    window.addEventListener('resize',()=>this.measure());
  }
  async load(){
    try{
      const [characters,effects]=await Promise.all([
        fetch('/assets/characters/manifest.json').then(r=>{if(!r.ok)throw Error();return r.json();}),
        fetch('/assets/effects/manifest.json').then(r=>{if(!r.ok)throw Error();return r.json();}),
      ]);
      for(const actor of characters.actors)this.actors.set(actor.key,actor);
      effects.assets.forEach((atlas,group)=>atlas.styles.forEach((style,row)=>this.effects.set(assetAliases[style]||style,{...atlas,group,row})));
      this.atlasUrls=effects.assets.map(atlas=>'/assets/effects/'+atlas.file);
      this.attach(this.root,this.context);this.schedule();
      return true;
    }catch{this.failed=true;this.onChange?.();return false;}
  }
  image(url){
    if(!this.images.has(url)){
      const image=new Image();image.decoding='async';
      const entry={image,loaded:false};this.images.set(url,entry);
      image.onload=()=>{entry.loaded=true;this.dirty=true;this.schedule();};
      image.onerror=()=>{this.failed=true;this.onChange?.();};image.src=url;
    }
    return this.images.get(url);
  }
  preload(state){
    if(!this.full)return;
    if(state&&this.atlasUrls&&!this.warmed){
      this.warmed=true;
      // Fetch in the background during deployment rather than on the first hit.
      this.atlasUrls.forEach((url,index)=>setTimeout(()=>this.image(url),index*80));
    }
    const roles=state?.view?.units?.map(u=>u.role)||state?.setup?.roles||[];
    const roleAssets=['Snipe','Bite','Shotgun','ShadowCut','HeavyCharge','Claw','KnightCharge',
      'CrimsonClaw','ArmigerThrust','Fire','GuardThrust','RoseBloom','SwordThrow'];
    for(const role of roles){
      const info=this.effects.get(roleAssets[role]);if(info)this.image('/assets/effects/'+info.file);
    }
    for(const name of ['PublicHit','HealthHit','ShieldHit']){
      const info=this.effects.get(name);if(info)this.image('/assets/effects/'+info.file);
    }
  }
  accept(previous,next){
    if(!next?.view||previous?.epoch!==next.epoch){this.reset();this.epoch=next?.epoch??-1;this.seen=next?.view?.revision??-1;return;}
    const packet=next.view.presentation;
    if(!packet||packet.revision<=this.seen||previous?.view?.revision===undefined)return;
    this.seen=packet.revision;
    // Initial reconnects restore the current state, rather than replaying old actions.
    if(next.view.revision!==previous.view.revision+1)return;
    this.queue.push({packet,state:next,before:previous.view});
    if(!this.active)this.start();
  }
  start(){
    const next=this.queue.shift();if(!next){this.active=null;return;}
    next.started=performance.now();next.lead=this.full?next.packet.lead/this.speed:0;
    next.phaseDuration=this.full?650/this.speed:120;
    const important=next.packet.notices.some(n=>n.weight>1);
    next.duration=this.full?Math.max((important?1800:next.packet.duration)/this.speed,
      next.lead+next.packet.phases.length*next.phaseDuration+220/this.speed):200;
    next.phase=-1;this.active=next;
    for(const effect of next.packet.phases){const info=this.effects.get(effectAsset(effect));if(info)this.image('/assets/effects/'+info.file);}
    this.schedule();
  }
  busy(){return !!this.active;}
  view(){return this.active?(performance.now()-this.active.started<this.active.lead?this.active.before:this.active.state.view):null;}
  lines(){return this.active?(performance.now()-this.active.started<this.active.lead?this.active.packet.notices.slice(0,1):this.active.packet.notices):null;}
  toggle(){this.full=!this.full;localStorage.setItem('mistforest-animation',this.full?'full':'simple');this.skip();this.onChange?.();}
  cycleSpeed(){this.speed=this.speed===1?1.25:this.speed===1.25?.8:1;localStorage.setItem('mistforest-animation-speed',String(this.speed));this.onChange?.();}
  reset(){this.queue=[];this.active=null;this.clearCanvas();}
  skip(){const wasBusy=this.busy();this.reset();if(wasBusy)this.onChange?.();}
  attach(root,context){
    this.root=root;this.context=context;if(!root)return;
    this.nodes=[...root.querySelectorAll('[data-actor-sprite]')];this.dirty=true;
    this.canvas=root.querySelector('.battle-effects');this.ctx=this.canvas?.getContext('2d');
    this.measure();
    const signature=context?.state?`${context.state.epoch}:${context.state.phase}`:'lobby';
    if(signature!==this.scene){this.scene=signature;this.mist();}
    this.preload(context?.state);this.schedule();
  }
  measure(){
    if(!this.canvas)return;
    const stage=this.canvas.parentElement,rect=stage.getBoundingClientRect(),dpr=Math.min(devicePixelRatio||1,2);
    this.canvas.width=Math.round(rect.width*dpr);this.canvas.height=Math.round(rect.height*dpr);
    this.width=rect.width;this.height=rect.height;this.ctx?.setTransform(dpr,0,0,dpr,0,0);
    this.cells.clear();
    for(const node of stage.querySelectorAll('[data-cell]')){
      const cell=node.getBoundingClientRect();this.cells.set(Number(node.dataset.cell),
        {x:cell.left-rect.left,y:cell.top-rect.top,width:cell.width,height:cell.height});
    }
  }
  mist(){
    document.querySelector('.scene-mist')?.remove();if(!this.full||document.hidden)return;
    const veil=document.createElement('div');veil.className='scene-mist';veil.setAttribute('aria-hidden','true');
    document.body.append(veil);setTimeout(()=>veil.remove(),1000);
    document.addEventListener('pointerdown',()=>veil.remove(),{once:true});
  }
  schedule(){if(!this.raf&&!document.hidden)this.raf=requestAnimationFrame(time=>this.tick(time));}
  tick(now){
    this.raf=null;if(document.hidden)return;
    const active=this.active,elapsed=active?now-active.started:0;
    if(active&&elapsed>=active.duration){this.start();this.onChange?.();}
    // Resource completion and fresh DOM need one paint even inside the animation
    // throttle window; reduced mode has no idle loop to retry a skipped paint.
    if(this.dirty||now-this.lastTick>=50){
      this.dirty=false;this.lastTick=now;this.animateActors(now);this.draw(now);
      if(this.active){
        const phase=Math.floor((now-this.active.started-this.active.lead)/this.active.phaseDuration);
        if(phase!==this.active.phase){this.active.phase=phase;queueMicrotask(()=>this.onChange?.());}
      }
    }
    if((this.full&&this.nodes.length)||this.active)this.schedule();
  }
  animateActors(now){
    const active=this.context?.historical?null:this.active;
    for(const node of this.nodes){
      const actor=this.actors.get(node.dataset.sprite);if(!actor)continue;
      const elapsed=active?now-active.started:0;
      const motion=active?.packet.movements.find(m=>m.unit===Number(node.dataset.unit)&&m.piece===Number(node.dataset.piece));
      const walking=this.full&&motion&&elapsed<active.lead;
      let clip=actor.clips.find(c=>c.kind===(walking?'walk':'idle'));
      let frameClock=walking?elapsed:now+actor.slot*193;
      // Native "special" clips are idle flourishes, not extra attacks.
      const flourish=actor.clips.find(c=>c.kind==='special'&&!c.placeholder);
      const idleClock=mod(now+actor.slot*193,15000);
      if(this.full&&!walking&&!active&&flourish&&idleClock>=10000&&idleClock<10000+flourish.durations_ms.reduce((sum,ms)=>sum+ms,0)){
        clip=flourish;frameClock=idleClock-10000;
      }
      if(!clip)continue;
      const entry=this.image('/'+clip.atlas),frame=this.full?frameAt(clip,frameClock):0;
      if(entry.loaded&&node.dataset.frame!==`${clip.kind}:${frame}`){
        node.style.backgroundImage=`url('/${clip.atlas}')`;
        node.style.backgroundSize=`${clip.frames*100}% 100%`;
        node.style.backgroundPosition=`${clip.frames>1?frame/(clip.frames-1)*100:0}% center`;
        node.style.bottom=`${((clip.ground_y??120)-120)/128*(node.classList.contains('multi')?73:96)}%`;
        node.dataset.frame=`${clip.kind}:${frame}`;
      }
      node.style.translate='';node.style.opacity='';node.style.rotate='';node.classList.remove('actor-hurt');
      if(walking)this.moveActor(node,motion,clamp(elapsed/active.lead));
      if(this.full&&active&&active.packet.actor===Number(node.dataset.unit)&&!walking&&elapsed<active.lead+320){
        node.style.rotate=`${Math.sin(clamp(elapsed/(active.lead+320))*Math.PI)*-5}deg`;
      }
      if(active&&elapsed>=active.lead){
        const phase=active.packet.phases[Math.min(active.packet.phases.length-1,Math.max(0,active.phase))];
        const cell=Number(node.closest('[data-cell]')?.dataset.cell);
        if(phase.health.includes(cell)||phase.shield.includes(cell))node.classList.add('actor-hurt');
        const fallen=node.closest('.fallen-actor');if(fallen)fallen.style.opacity=String(1-clamp((elapsed-active.lead)/700));
      }
    }
  }
  moveActor(node,motion,t){
    const destination=this.cells.get(Number(node.closest('[data-cell]')?.dataset.cell)),start=this.cells.get(motion.from);if(!destination||!start)return;
    if(motion.style===3){
      const end=this.cells.get(motion.to);if(!end)return;
      node.style.translate=t<.5?`${start.x-destination.x}px ${start.y-destination.y}px`:`${end.x-destination.x}px ${end.y-destination.y}px`;
      node.style.opacity=String(Math.abs(2*t-1));return;
    }
    const state=this.active.state,w=state.map.width,h=state.map.height,path=motion.path;
    const progress=ease(t)*(path.length-1),segment=Math.min(path.length-2,Math.floor(progress)),fraction=progress-segment;
    if(segment<0)return;
    const a=path[segment],b=path[segment+1],x=a[0]+(b[0]-a[0])*fraction,y=a[1]+(b[1]-a[1])*fraction;
    const first=this.cells.get(1),last=this.cells.get(w*h);if(!first||!last)return;
    const dx=w>1?(last.x-first.x)/(w-1):first.width,dy=h>1?(last.y-first.y)/(h-1):first.height;
    const lift=motion.style===2?Math.sin(t*Math.PI)*first.height*.4:0;
    node.style.translate=`${first.x+mod(x,w)*dx-destination.x}px ${first.y+mod(y,h)*dy-destination.y-lift}px`;
    if(x<0||y<0||x>=w||y>=h)node.style.opacity='.45';
  }
  frame(name,index){
    const key=`${name}:${index}`;if(this.frames.has(key))return this.frames.get(key);
    const info=this.effects.get(name);if(!info)return null;
    const entry=this.image('/assets/effects/'+info.file);if(!entry.loaded)return null;
    const image=entry.image,left=Math.floor(index*info.width/4),right=Math.floor((index+1)*info.width/4);
    const top=rowCuts[info.group]?.[info.row]??Math.floor(info.row*info.height/info.rows);
    const bottom=rowCuts[info.group]?.[info.row+1]??Math.floor((info.row+1)*info.height/info.rows);
    const canvas=document.createElement('canvas');canvas.width=right-left;canvas.height=bottom-top;
    const ctx=canvas.getContext('2d',{willReadFrequently:true});ctx.drawImage(image,left,top,canvas.width,canvas.height,0,0,canvas.width,canvas.height);
    if(info.group>0){
      const pixels=ctx.getImageData(0,0,canvas.width,canvas.height);
      for(let y=0;y<canvas.height;y++)for(let x=0;x<canvas.width;x++){
        const edge=Math.min(x,y,canvas.width-1-x,canvas.height-1-y),at=(y*canvas.width+x)*4+3;
        pixels.data[at]=pixels.data[at]<4?0:pixels.data[at]*ease((edge-4)/10);
      }
      ctx.putImageData(pixels,0,0);
    }
    this.frames.set(key,canvas);return canvas;
  }
  painted(name,cell,t,alpha=1,scale=1){
    const rect=this.cells.get(cell);if(!rect||!name)return false;
    const frameProgress=clamp(t/.65)*3,first=Math.min(3,Math.floor(frameProgress)),next=Math.min(3,first+1),blend=ease(frameProgress-first);
    const a=this.frame(name,first),b=this.frame(name,next);if(!a||!b)return false;
    const size=Math.min(rect.width,rect.height)*scale,x=rect.x+(rect.width-size)/2,y=rect.y+(rect.height-size)/2;
    this.ctx.save();this.ctx.imageSmoothingEnabled=true;
    for(const [image,opacity] of [[a,1-blend],[b,blend]]){this.ctx.globalAlpha=alpha*opacity;this.ctx.drawImage(image,x,y,size,size);}
    this.ctx.restore();return true;
  }
  clearCanvas(){this.ctx?.clearRect(0,0,this.width||0,this.height||0);}
  draw(now){
    this.clearCanvas();if(!this.active||!this.full||!this.ctx||this.context?.historical)return;
    const active=this.active,elapsed=now-active.started-active.lead;if(elapsed<0)return;
    const index=Math.min(active.packet.phases.length-1,Math.floor(elapsed/active.phaseDuration));
    const phase=active.packet.phases[index],t=(elapsed-index*active.phaseDuration)/active.phaseDuration;if(t>1)return;
    const alpha=Math.sin(Math.PI*clamp(t))*.9+.1*(1-t),asset=effectAsset(phase);
    for(const cell of phase.cells){
      if(phase.style===13&&!phase.attacks.includes(cell)&&!phase.buds.includes(cell))continue;
      if(!this.painted(phase.buds.includes(cell)?'RosePlant':asset,cell,t,alpha))this.fallback(cell,t,phase);
    }
    for(const cell of phase.petals)this.painted('RosePetals',cell,t,alpha);
    for(const cell of phase.wither)this.painted('RoseWither',cell,t,alpha);
    for(const cell of phase.shield)this.painted('ShieldHit',cell,t,alpha,.65);
    for(const cell of phase.health)this.painted('HealthHit',cell,t,alpha,.5);
    for(const cell of phase.impact)this.painted('PublicHit',cell,t,alpha,.65);
    for(const cell of phase.health)this.damage(cell,t);
  }
  fallback(cell,t,phase){
    const rect=this.cells.get(cell);if(!rect||!phase.style)return;
    const ctx=this.ctx,x=rect.x+rect.width/2,y=rect.y+rect.height/2,r=rect.width*.3*ease(t/.5);
    ctx.save();ctx.globalAlpha=1-t;ctx.strokeStyle=[11,27,29].includes(phase.style)?'#dc753b':'#b3914c';ctx.lineWidth=3;
    ctx.beginPath();ctx.arc(x,y,r,0,Math.PI*2);ctx.stroke();ctx.restore();
  }
  damage(cell,t){
    const rect=this.cells.get(cell);if(!rect)return;
    const before=this.active.before,after=this.active.state.view;let damage=0;
    for(const unit of before.units){
      if(Math.floor(unit.id/2)!==after.player)continue;
      const next=after.units[unit.id];
      if(unit.role===10)unit.soldiers.forEach((position,p)=>{if(position===cell)damage+=Math.max(0,unit.soldierHealth[p]-next.soldierHealth[p]);});
      else if(unit.position===cell)damage+=Math.max(0,unit.health-next.health);
    }
    if(!damage)return;
    const ctx=this.ctx;ctx.save();ctx.globalAlpha=1-t*.75;ctx.font=`bold ${Math.max(16,rect.width*.26)}px Georgia,serif`;
    ctx.textAlign='center';ctx.lineWidth=4;ctx.strokeStyle='#fffaf0';ctx.fillStyle='#a94c3b';
    const x=rect.x+rect.width/2,y=rect.y+rect.height*.3-t*rect.height*.35;
    const text=`−${Number(damage.toFixed(2))}`;ctx.strokeText(text,x,y);ctx.fillText(text,x,y);ctx.restore();
  }
}
