import {Presentation} from './presentation.mjs';
import {BattleLog} from './battle-log.mjs';
const app=document.querySelector('#app'),dialog=document.querySelector('#dialog'),toast=document.querySelector('#toast');
const $=selector=>document.querySelector(selector);
const esc=value=>String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let catalog,state,session,connected=false,busy=false,streamController,streamGeneration=0;
let slot=0,piece=0,roles=[0,1],cells=[0,0],formations=[[0,0,0],[0,0,0]];
let unit=-1,kind=-1,target=null,variant=0,historyView=null,toastTimer;
const visuals=new Presentation(()=>{if(catalog)render();});
const battleLog=new BattleLog(document.querySelector('#battle-log'),{
  onRead:updateLogBadge,
  onHistory:step=>perform(async()=>{const answer=await api(endpoint('history')+'?step='+step);historyView=answer.view;target=null;battleLog.dialog.close();render();}),
  onLive:()=>{historyView=null;kind=-1;target=null;battleLog.dialog.close();render();},
  onClose:()=>document.getElementById('log-trigger')?.focus({preventScroll:true}),
});
const dockObserver=new ResizeObserver(()=>{
  const dock=app.querySelector('.control-panel,.setup-panel'),result=app.querySelector('.result-banner');
  app.style.setProperty('--dock-height',(dock?.getBoundingClientRect().height||0)+'px');
  app.style.setProperty('--result-height',(result?.getBoundingClientRect().height||0)+'px');
  requestAnimationFrame(()=>visuals.measure());
});
function updateLogBadge(){
  const count=battleLog.unread(),button=document.getElementById('log-trigger');
  if(!button)return;
  button.setAttribute('aria-label',count?'战报，'+count+' 条未读':'查看战报');
  button.querySelector('.unread-badge').textContent=count?String(Math.min(count,99)):'';
}
const immediate=new Set([5,8,11,18,19,20,26,28,35]);
try{session=JSON.parse(sessionStorage.getItem('mistforest-seat')||'null');}catch{session=null;}
function message(text){toast.textContent=text;toast.classList.add('visible');clearTimeout(toastTimer);toastTimer=setTimeout(()=>toast.classList.remove('visible'),4200);}
function remember(){if(session)sessionStorage.setItem('mistforest-seat',JSON.stringify(session));else sessionStorage.removeItem('mistforest-seat');}
async function api(url,data,auth=true){
  const headers={};if(data!==undefined)headers['Content-Type']='application/json';if(auth&&session)headers.Authorization=`Bearer ${session.token}`;
  const response=await fetch(url,{method:data===undefined?'GET':'POST',headers,body:data===undefined?undefined:JSON.stringify(data),signal:AbortSignal.timeout(18000)});
  const result=await response.json();if(!response.ok||!result.ok)throw Object.assign(new Error(result.error||'连接失败，请重试。'),{status:response.status});return result;
}
const endpoint=name=>`/api/rooms/${session.code}/${name}`;
const roleInfo=id=>catalog.roles[id];
const portrait=id=>`/assets/portraits/${roleInfo(id).portrait}.png`;
const coord=cell=>cell>0?`(${Math.floor((cell-1)/(state?.map.width||5))+1},${(cell-1)%(state?.map.width||5)+1})`:'未选择';
function header(){return `<header class="topbar"><a class="brand" href="/" data-command="home">雾隐之森</a><div class="top-actions">${state?`<span class="connection ${connected?'':'offline'}">${connected?'房间 '+state.code:'正在重连'}</span><button class="quiet menu-button" data-command="room-menu" aria-label="房间与设置"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="5" cy="12" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="19" cy="12" r="1.5"/></svg></button>`:'<button class="quiet" data-command="book">角色图鉴</button>'}</div></header>`;}

function instructions(){return `<details class="instructions"><summary>玩法与操作</summary><p>双方只能看见自己的位置。根据公开攻击和私人反馈推理对手的落点，消灭对方角色获胜。</p><ul><li>部署：选择角色槽，再点棋盘放置起点；守卫军需要放置三名士兵。双方都准备后自动开局，先手随机。</li><li>行动：选择己方角色与动作，点选圆点标记的目标格，再确认执行。每回合只使用一名角色；行动额度由原版规则裁定。</li><li>特殊技能可在“动作参数”切换目标、弹药、士兵或落点。祝融和长生剑攻击后，必须先完成秘密移动选择。</li><li>结束回合会放弃剩余行动。手机切到后台或短暂断线后，返回页面会自动同步。</li></ul><p>刷新会保留本标签页的席位。不同设备打开同一个游戏网址，用房间号加入；使用公网网址时，无需连接同一 Wi-Fi。服务器重启后房间清空。</p></details>`;}
function lobby(){
  const invited=new URLSearchParams(location.search).get('room')||'';
  return `${header()}<main class="lobby"><section class="welcome"><h1>雾隐之森</h1><p class="lead">看不见对手，读懂每一步。</p><p>选好你的角色，藏住自己的位置。与好友在同一片森林里，展开一场关于判断与试探的对决。</p><div class="character-line" aria-hidden="true">${[4,0,2,12].map(id=>`<img src="${portrait(id)}" alt="">`).join('')}</div><button class="quiet" data-command="rules">第一次来？查看玩法</button></section><section class="entry"><h2>与好友开一局</h2><p class="muted">创建房间，把房间号告诉你的对手。</p><form id="create-form"><label for="nickname">你的称呼</label><input id="nickname" name="name" maxlength="16" placeholder="旅人" autocomplete="nickname" value="${esc(localStorage.getItem('mistforest-name')||'')}"><div class="field-row"><div><label for="players">玩家人数</label><select id="players" name="players"><option value="2">2 人对局</option><option value="3">3 人混战</option><option value="4">4 人混战</option></select></div><div><label for="slots">每方角色</label><select id="slots" name="slots"><option value="2">双角色</option><option value="1">单角色</option></select></div></div><label for="map">选择地图</label><select id="map" name="map">${catalog.maps.map(m=>`<option value="${esc(m.id)}">${esc(m.name)} · ${m.width}×${m.height}</option>`).join('')}</select><button class="btn primary" type="submit" ${busy?'disabled':''}>创建房间</button></form><div class="divider">已有房间号</div><form id="join-form" class="join-row"><input name="code" aria-label="六位房间号" placeholder="输入房间号" maxlength="6" pattern="[a-fA-F0-9]{6}" required value="${esc(invited)}"><button class="btn" type="submit" ${busy?'disabled':''}>加入房间</button></form><p class="muted small" style="margin-top:13px">电脑和手机打开同一个游戏网址，用房间号加入。使用公网网址时，无需连接同一 Wi-Fi。</p></section></main><footer class="lobby-footer"><span>十三职业 · 秘密部署 · 逐步裁定</span><span>网页版 ${esc(catalog.version)} · 网页联机版</span></footer>`;
}
function seatLine(){return `<div class="seats">${state.seats.map((s,i)=>s?`<span class="seat ${i===state.you?'mine':''}">P${i+1} ${esc(s.name)}${i===state.you?'（你）':''} · ${s.online?'在线':'离线'}${state.phase==='setup'?(s.ready?' · 已准备':' · 部署中'):''}</span>`:`<span class="seat empty">P${i+1} 等待加入</span>`).join('')}</div>`;}
function roomStrip(){return `<div class="room-strip"><div><div class="room-code"><h1>房间</h1><strong>${state.code}</strong><button class="quiet" data-command="invite">邀请好友</button></div>${seatLine()}</div><button class="textbutton" data-command="leave">离开房间</button></div>`;}
function sprite(u,multi=false,piece=0,index=0){const key=u.role===5&&u.form===0?'hidden-human':roleInfo(u.role).sprite;return `<span class="sprite ${multi?'multi':''}" data-actor-sprite data-sprite="${key}" data-unit="${u.id}" data-piece="${piece}" style="background-image:url('/assets/characters/${key}/idle.png');${multi?'left:'+(index%2?28:-2)+'%;':''}"></span>`;}
function board(setup=false){
  const map=state.map,v=currentView();
  const ready=!!state.setup;
  const options=setup?[]:currentActions();
  const legal=new Set(options.map(a=>a[2]));
  let html='';
  for(let c=1;c<=map.width*map.height;c++) {
    const row=Math.floor((c-1)/map.width),column=(c-1)%map.width;
    const terrain=map.cells[c-1],classes=['cell',['ground','wall','lake','whirlpool'][terrain]];
    if(map.horizontal[row*map.width+column])classes.push('edge-top');
    if(map.horizontal[(row+1)*map.width+column])classes.push('edge-bottom');
    if(map.vertical[row*(map.width+1)+column])classes.push('edge-left');
    if(map.vertical[row*(map.width+1)+column+1])classes.push('edge-right');
    let content='',selected=false;
    if(setup) {
      for(let s=0;s<state.config.slots;s++) {
        const spots=roles[s]===10?formations[s]:[cells[s]];
        spots.forEach((cell,k)=>{if(cell===c){content+=`<span class="deployment-actor ${ready?'':'unconfirmed'}">${sprite({id:state.you*2+s,role:roles[s],form:1},false,k)}</span><span class="deployment-marker">${s+1}${roles[s]===10?String.fromCharCode(65+k):''}</span>`;if(s===slot&&k===piece)selected=true;}});
      }
      if(!ready&&[0,3].includes(terrain))classes.push('legal');
    } else {
      const occupants=[];
      for(const u of v.units)if(Math.floor(u.id/2)===state.you&&u.health>0&&u.id%2<state.config.slots){
        const positions=u.role===10?u.soldiers:[u.position];
        positions.forEach((cell,p)=>{if(cell===c&&(u.role!==10||u.soldierHealth[p]>0))occupants.push({u,p});});
      }
      if(!historyView&&visuals.active){
        for(const old of visuals.active.before.units){
          if(Math.floor(old.id/2)!==state.you||old.health<=0||v.units[old.id]?.health>0)continue;
          const positions=old.role===10?old.soldiers:[old.position];
          positions.forEach((cell,p)=>{if(cell===c&&(old.role!==10||old.soldierHealth[p]>0))content+=`<span class="fallen-actor" style="opacity:${Math.max(0,1-(performance.now()-visuals.active.started-visuals.active.lead)/700)}">${sprite(old,false,p)}</span>`;});
        }
      }
      content+=occupants.map(({u,p},i)=>sprite(u,occupants.length>1,p,i)).join('');
      if(occupants.length)content+=`<span class="unit-marker">${occupants.map(({u,p})=>`${u.id%2+1}${u.role===10?String.fromCharCode(65+p):''}`).join(' / ')}</span>`;
      if(v.buildings[c-1][0])content+=`<span class="structure structure-${v.buildings[c-1][0]}" aria-hidden="true"></span><span class="object-marker structure-label">${['','堡','墙','城'][v.buildings[c-1][0]]}</span>`;
      if(v.fires[c-1]>=0)content+='<span class="object-marker">火</span>';
      if(v.roses.some(r=>r[1]===c&&r[2]>0))content+='<span class="object-marker">❀</span>';
      if(v.swords.some(s=>s[1]===c))content+='<span class="object-marker">剑</span>';
      if(!historyView&&!immediate.has(kind)&&legal.has(c))classes.push('legal');
      if(v.enemyAttacks[c-1])classes.push('attacked');
      selected=target===c&&!historyView;
    }
    if(content.includes('data-actor-sprite'))classes.push('occupied');
    if(selected)classes.push('selected');
    const label=`第${row+1}行第${column+1}列 ${['空地','方形墙','湖泊','漩涡'][terrain]}`;
    html+=`<button class="${classes.join(' ')}" data-cell="${c}" aria-label="${label}" aria-pressed="${selected}" ${setup&&ready?'disabled':''}><span class="tile"></span>${content}${terrain&&!content?`<span class="terrain-name">${['','方形墙','湖泊','漩涡'][terrain]}</span>`:''}</button>`;
  }
  return `<div class="board-area"><div class="axis-top" aria-hidden="true" style="--cols:${map.width}">${Array.from({length:map.width},(_,i)=>'<span>'+(i+1)+'</span>').join('')}</div><div class="axis-left" aria-hidden="true" style="--rows:${map.height}">${Array.from({length:map.height},(_,i)=>'<span>'+(i+1)+'</span>').join('')}</div><div class="board-stage"><div class="board" role="group" aria-label="${setup?'秘密部署棋盘':'对局棋盘'}" style="--cols:${map.width}">${html}</div><canvas class="battle-effects" aria-hidden="true"></canvas></div></div><output class="board-cursor sr-only"></output>`;
}
function setupPanel(){
  return `<aside class="setup-panel"><div class="setup-slots">${Array.from({length:state.config.slots},(_,s)=>`<div class="slot-wrap"><button class="slot ${slot===s?'active':''}" data-slot="${s}" aria-pressed="${slot===s}" ${state.setup?'disabled':''}><img src="${portrait(roles[s])}" alt=""><span><strong>${esc(roleInfo(roles[s]).name)}</strong><small>${roles[s]===10?formations[s].filter(Boolean).length+'/3 名士兵':cells[s]?coord(cells[s]):'待放置'}</small></span></button><button class="textbutton role-switch" data-pick-role="${s}" ${state.setup?'disabled':''}>更换</button></div>`).join('')}</div>${roles[slot]===10?`<div class="segmented">${[0,1,2].map(p=>`<button class="${piece===p?'active':''}" data-piece="${p}" ${state.setup?'disabled':''}>${String.fromCharCode(65+p)} ${formations[slot][p]?coord(formations[slot][p]):'待放置'}</button>`).join('')}</div>`:''}<p class="setup-hint">${state.setup?'等待其他玩家准备':'选角色，再点棋盘放置起点'}</p><button class="quiet" data-role-help="${roles[slot]}">角色详情</button><button class="btn primary ready-btn" data-command="ready" ${busy?'disabled':''}>${state.setup?'调整部署':'确认部署'}</button></aside>`;
}

function currentView(){return historyView||visuals.view()||state?.view;}
function unitActions(){return historyView?[]:(state?.view?.actions||[]).filter(a=>a[0]===unit);}
function variantKey(a){return [a[3],a[4],a[5],a[6],a[7],immediate.has(kind)?a[2]:''].join(',');}
function variants(){const unique=new Map();for(const a of unitActions().filter(a=>a[1]===kind))unique.set(variantKey(a),a);return [...unique.values()];}
function currentActions(){const all=unitActions().filter(a=>a[1]===kind),values=variants();if(!values.length)return [];const key=variantKey(values[Math.min(variant,values.length-1)]);return all.filter(a=>variantKey(a)===key);}
function variantLabel(a){
  const labels=[];
  if(currentView().units[unit]?.role===10)labels.push(`士兵 ${String.fromCharCode(65+a[4])}`);
  if([5,6].includes(a[1]))labels.push(`目标 P${Math.floor(a[3]/2)+1} ${roleInfo(currentView().units[a[3]].role).name}${currentView().units[a[3]].role===10?'·士兵'+String.fromCharCode(65+a[5]):''}`);
  if(a[1]===8)labels.push(a[2]?'变为狼形':'变为人形');
  if(a[1]===16&&a[5]>0)labels.push(`另一段墙 ${coord(a[5])}`);
  if(a[1]===20)labels.push(`卸甲选项 ${a[6]}`);
  if(a[1]===22)labels.push(`离火 × ${a[6]}`);
  if(a[1]===23)labels.push(a[6]?'位移并点燃':'仅位移');
  if(a[1]===28)labels.push(a[6]===1?'带动模式':'跟随模式');
  if(a[1]===30)labels.push(`玫瑰 ${coord(a[5])} · 落点 ${coord(a[7])}`);
  if(a[1]===31)labels.push(a[6]?'包含自身格':'不含自身格');
  if(a[1]===32)labels.push(`剑气消耗 ${a[6]}`);
  return labels.join(' · ')||'默认';
}
function prepareActions(){
  const v=currentView();if(!v)return;
  const own=v.units.filter(u=>Math.floor(u.id/2)===state.you&&u.id%2<state.config.slots&&u.health>0);
  if(!own.some(u=>u.id===unit))unit=own[0]?.id??state.you*2;
  if(v.budget.active>=0&&Math.floor(v.budget.active/2)===state.you)unit=v.budget.active;
  const available=[...new Set(unitActions().map(a=>a[1]))].filter(k=>![26,35].includes(k));
  if(!available.includes(kind)) {kind=available.includes(1)?1:available[0]??-1;target=null;variant=0;}
  if(variant>=variants().length)variant=0;
}
function battlePanel(){
  const v=currentView(),myturn=!historyView&&!visuals.busy()&&v.inputPlayer===state.you&&v.winner===-1;
  const own=v.units.filter(u=>Math.floor(u.id/2)===state.you&&u.id%2<state.config.slots);
  const available=[...new Set(unitActions().map(a=>a[1]))].filter(k=>![26,35].includes(k));
  const options=variants(),choices=currentActions(),selected=immediate.has(kind)?choices[0]:choices.find(a=>a[2]===target);
  const canEnd=myturn&&unitActions().some(a=>a[1]===35);
  const quick=available.length>4?[...available.slice(0,2),available.includes(kind)&&!available.slice(0,2).includes(kind)?kind:available[2]]:available;
  return `<aside class="control-panel" aria-label="角色与行动"><h2 class="sr-only">${historyView?'历史视图':visuals.busy()?'正在显示行动判定':v.winner!==-1?'本局结束':myturn?'轮到你行动':'等待对手行动'}</h2><div class="battle-roster">${own.map(u=>`<button class="unit-card ${u.id===unit?'active':''} ${u.health<=0?'dead':''}" data-unit="${u.id}" aria-pressed="${u.id===unit}" ${u.health<=0?'disabled':''}><img src="${portrait(u.role)}" alt=""><span><strong>${esc(roleInfo(u.role).name)}</strong><small>${u.health<=0?'已退场':'生命 '+Number(u.health.toFixed(2))}${u.armor?' · 甲'+u.armor:''}${u.shield?' · 盾'+u.shield:''}</small></span></button>`).join('')}</div><div class="action-budget"><span>${myturn?'移动 '+v.budget.moves+' · 攻击 '+v.budget.attacks:historyView?'历史第 '+v.revision+' 步':visuals.busy()?'正在显示判定':v.winner!==-1?'本局结束':'等待 P'+(v.inputPlayer+1)}</span><button class="textbutton" data-command="unit-info">角色详情</button></div>${myturn?`<div class="action-grid" role="group" aria-label="选择动作">${quick.map(k=>`<button class="${kind===k?'active':''}" data-kind="${k}" aria-pressed="${kind===k}">${esc(catalog.kinds[k])}</button>`).join('')}${available.length>4?'<button data-command="all-actions">更多动作</button>':''}</div>${options.length>1?`<div class="action-params"><label for="variant">动作参数</label><select id="variant">${options.map((a,i)=>`<option value="${i}" ${i===variant?'selected':''}>${esc(variantLabel(a))}</option>`).join('')}</select></div>`:''}${!immediate.has(kind)&&choices.some(a=>a[2]===0)?`<button class="quiet stay-button" data-target="0" aria-pressed="${target===0}">${target===0?'已选：':''}留在原地</button>`:''}<div class="execute"><p class="target-label">${v.budget.followup?'请完成攻击后的秘密移动':kind<0?'暂无可用动作':immediate.has(kind)?esc(catalog.kinds[kind]):target===null?'请点选目标':target===0?'留在原地':esc(catalog.kinds[kind])+' '+coord(target)}</p><button class="btn primary" data-command="execute" ${!selected||busy?'disabled':''}>确认${kind>=0?esc(catalog.kinds[kind]):'执行'}</button><button class="textbutton end-turn" data-command="end" ${!canEnd||busy?'disabled':''}>结束回合</button></div>`:`<div class="waiting">${historyView?'<button class="btn primary" data-command="live">回到当前</button>':visuals.busy()?'<button class="quiet" data-command="skip-playback">跳过演出</button>':'<span>可查看战报与角色状态</span>'}</div>`}</aside>`;
}

function visibleNotices(v){
  const lines=visuals.lines();
  return lines&&!historyView?v.notices.filter(n=>n.revision<visuals.active.packet.revision).concat(lines.map(n=>({...n,revision:visuals.active.packet.revision}))):v.notices;
}
function verdict(v){
  const lines=!historyView&&visuals.lines(),latest=lines?.[lines.length-1];
  const text=latest?.text||v.feedback||v.result||(v.inputPlayer===state.you?'选择动作，再点目标':'等待公开判定');
  return '<button class="arena-verdict" data-command="log" aria-label="查看完整判定与战报"><span class="sr-only">'+(latest?.feedback?'仅你可见：':'行动判定：')+'</span><span>'+esc(text)+'</span></button>';
}

function visualControls(){return '<div class="visual-controls"><button class="quiet" data-command="toggle-animation">动画：'+(visuals.full?'完整':'简化')+'</button><button class="quiet" data-command="animation-speed">速度：'+(visuals.speed===1?'正常':visuals.speed>1?'较快':'较慢')+'</button>'+(visuals.busy()?'<button class="quiet" data-command="skip-playback">跳过演出</button>':'')+(visuals.failed?'<span class="asset-warning">部分画面未加载，请刷新重试</span>':'')+'</div>';}
function roomScreen(){
  const setup=state.phase==='setup',v=currentView();if(v)prepareActions();
  const winning=v&&v.winner!==-1&&!visuals.busy();
  const winnerName=winning?(v.winner<0?'本局平局':state.config.teams?`队伍 ${v.winner%2+1} 获胜`:`P${v.winner+1} ${state.seats[v.winner]?.name||''} 获胜`):'';
  return `${header()}<main class="room ${setup?'deployment-layout':'battle-layout'}" style="--map-ratio:${state.map.width/state.map.height}">${winning?`<section class="result-banner"><h2>${esc(winnerName)}</h2>${state.you===0?'<button class="btn" data-command="rematch">再开一局</button>':'<span>等待房主再开一局</span>'}</section>`:''}<section class="battlefield"><div class="board-heading"><div class="phase-heading"><h2>${setup?'秘密部署':historyView?'第 '+v.revision+' 步':'第 '+v.round+' 轮'}</h2><span class="pill">${setup?state.setup?'已准备':'仅自己可见':historyView?'历史视角':visuals.busy()?'正在判定':v.winner!==-1?'本局结束':v.inputPlayer===state.you?'你的回合':'P'+(v.inputPlayer+1)+' 行动'}</span></div>${setup?'<button class="quiet" data-command="rules">玩法</button>':'<button id="log-trigger" class="quiet log-trigger" data-command="log" aria-haspopup="dialog">战报 <span class="unread-badge"></span></button>'}</div>${board(setup)}<div class="game-feedback">${setup?'<span>选中角色后，点棋盘放置起点</span>':verdict(v)}${!setup&&!historyView&&visuals.busy()?'<button class="textbutton" data-command="skip-playback">跳过</button>':''}</div></section>${setup?setupPanel():battlePanel()}</main>`;
}

function render(){
  const focus=document.activeElement?.id,openDetails=[...app.querySelectorAll('details[open]')].map(d=>d.className);
  dockObserver.disconnect();app.classList.toggle('in-room',!!state);
  app.innerHTML=state?roomScreen():lobby();
  const dock=app.querySelector('.control-panel,.setup-panel');if(dock)dockObserver.observe(dock);
  const result=app.querySelector('.result-banner');if(result)dockObserver.observe(result);
  if(state?.view){const live=visuals.view()||state.view;battleLog.update({view:{...live,notices:visibleNotices(live)},key:state.code+':'+state.epoch,revision:state.view.revision,historical:historyView?.revision??null,busy:visuals.busy()});updateLogBadge();}
  for(const d of app.querySelectorAll('details'))if(openDetails.includes(d.className))d.open=true;
  if(focus)document.getElementById(focus)?.focus({preventScroll:true});
  visuals.attach(app,{state,historical:!!historyView});
}
function receive(next){
  if(next.closed){clearSession();message('房主已关闭房间。');return;}
  if(state&&next.epoch===state.epoch&&next.sequence<state.sequence)return;
  const previous=state;
  const reset=!state||state.epoch!==next.epoch;
  if(reset){battleLog.dialog.close();dialog.close();slot=piece=0;roles=[0,1];cells=[0,0];formations=[[0,0,0],[0,0,0]];unit=-1;kind=-1;historyView=null;}
  if(next.setup){roles=[...next.setup.roles];cells=[...next.setup.cells];formations=next.setup.formations.map(f=>[...f]);}
  if(!state||state.view?.revision!==next.view?.revision||reset){target=null;variant=0;}
  state=next;visuals.accept(previous,next);render();
}
function clearSession(){battleLog.dialog.close();dialog.close();streamGeneration++;streamController?.abort();session=null;state=null;connected=false;visuals.reset();remember();render();}
async function connect(){
  const generation=++streamGeneration;streamController?.abort();
  while(session&&generation===streamGeneration){
    streamController=new AbortController();
    try{
      const response=await fetch(endpoint('events'),{headers:{Authorization:`Bearer ${session.token}`},signal:streamController.signal});
      if(!response.ok){const answer=await response.json();throw Object.assign(new Error(answer.error),{status:response.status});}
      connected=true;render();const reader=response.body.getReader(),decoder=new TextDecoder();let buffer='';
      while(true){const {value,done}=await reader.read();if(done)break;buffer+=decoder.decode(value,{stream:true});let end;
        while((end=buffer.indexOf('\n\n'))>=0){const packet=buffer.slice(0,end);buffer=buffer.slice(end+2);if(packet.startsWith('data: '))receive(JSON.parse(packet.slice(6)));}
      }
    }catch(error){if(generation!==streamGeneration)return;if(error.status===401||error.status===404){clearSession();message(error.message);return;}}
    if(generation!==streamGeneration)return;connected=false;render();await new Promise(resolve=>setTimeout(resolve,1800));
  }
}
async function enter(answer){session={code:answer.code,token:answer.token};remember();connected=false;state=null;receive(answer.state);history.replaceState(null,'',`/?room=${session.code}`);connect();}
async function perform(work){if(busy)return;busy=true;try{await work();}catch(error){message(error.message||'连接失败，请重试。');}finally{busy=false;render();}}
function openDialog(content){dialog.innerHTML=content;if(!dialog.open)dialog.showModal();}
function dialogHeader(title){return `<div class="dialog-header"><h2>${esc(title)}</h2><button class="quiet" data-command="close-dialog" aria-label="关闭窗口">关闭</button></div>`;}
function showRoomMenu(){
  openDialog(`${dialogHeader('房间与设置')}${roomStrip()}<div class="menu-section"><button class="btn" data-command="invite">邀请好友</button></div><div class="menu-section">${visualControls()}<button class="textbutton" data-command="book">角色图鉴</button><button class="textbutton" data-command="rules">玩法与操作</button>${state.view?.actions?.some(a=>a[1]===26)?'<button class="textbutton danger" data-command="surrender">投降</button>':''}</div><p class="muted">服务器重启后房间清空。网页版 ${esc(catalog.version)}</p>`);
}
function showActions(){
  const available=[...new Set(unitActions().map(a=>a[1]))].filter(k=>![26,35].includes(k));
  openDialog(`${dialogHeader('选择动作')}<div class="all-actions">${available.map(k=>`<button class="quiet ${kind===k?'active':''}" data-kind="${k}" aria-pressed="${kind===k}">${esc(catalog.kinds[k])}</button>`).join('')}</div>`);
}
function showUnitInfo(){
  const v=currentView();
  openDialog(`${dialogHeader('角色与情报')}<div class="unit-info">${v.units.filter(u=>u.id%2<state.config.slots).map(u=>`<section><button class="unit-info-heading" data-role-help="${u.role}"><img src="${portrait(u.role)}" alt=""><span>P${Math.floor(u.id/2)+1} · ${esc(roleInfo(u.role).name)}<small>生命 ${Number(u.health.toFixed(2))}${u.armor?' · 甲'+u.armor:''}${u.shield?' · 盾'+u.shield:''}</small></span><span>查看技能</span></button><p>${Math.floor(u.id/2)!==state.you?'位置隐藏':u.health<=0?'已退场':u.role===10?u.soldiers.map((c,i)=>String.fromCharCode(65+i)+' '+coord(c)+' 生命 '+u.soldierHealth[i]).join('；'):'位置 '+coord(u.position)}</p>${u.report?'<p class="private-report">'+esc(u.report)+'</p>':''}${u.silenced?'<p>当前被沉默</p>':''}</section>`).join('')}</div>`);
}
function showBook(id=0){const r=roleInfo(id);openDialog(`${dialogHeader('角色图鉴')}<nav class="role-nav" aria-label="选择职业">${catalog.roles.map(r=>`<button data-role-help="${r.id}" class="${r.id===id?'active':''}">${esc(r.name)}</button>`).join('')}</nav><div class="role-detail"><img class="portrait" src="${portrait(id)}" alt="${esc(r.name)}"><div><h3>${esc(r.name)} · ${esc(r.skill)}</h3><p class="stats">${esc(r.stats)}</p><p class="preline">${esc(r.help)}</p><p class="preline muted" style="margin-top:16px">${esc(r.tips)}</p></div></div>`);}
function showRolePicker(s){openDialog(`${dialogHeader(`选择第 ${s+1} 名角色`)}<div class="role-grid">${catalog.roles.map(r=>`<button data-role-pick="${r.id}" data-role-slot="${s}" ${roles.some((id,i)=>i!==s&&id===r.id)?'disabled':''}><img src="${portrait(r.id)}" alt=""><strong>${esc(r.name)}</strong><small>${esc(r.skill)}</small></button>`).join('')}</div><p class="muted" style="margin-top:14px">每方职业不能重复。完整技能说明可在顶部角色图鉴查看。</p>`);}
async function sendAction(action){
  if(visuals.busy()){message('请等待当前判定结束，或点击“跳过演出”。');return;}
  const answer=await api(endpoint('action'),{id:crypto.randomUUID?.()||`${Date.now()}-${Math.random().toString(36).slice(2)}`,epoch:state.epoch,revision:state.view.revision,action});
  receive(answer.state);
}
app.addEventListener('submit',event=>{
  event.preventDefault();const form=event.target,values=Object.fromEntries(new FormData(form));
  perform(async()=>{
    const name=$('#nickname')?.value.trim()||'旅人';localStorage.setItem('mistforest-name',name);
    if(form.id==='create-form')await enter(await api('/api/rooms',{name,players:Number(values.players),slots:Number(values.slots),map:values.map},false));
    else if(form.id==='join-form')await enter(await api(`/api/rooms/${values.code.trim().toUpperCase()}/join`,{name},false));
  });
});
function clicked(event){
  const element=event.target.closest('button,a[data-command]');if(!element)return;
  if(element.disabled)return;
  if(element.dataset.roleHelp!==undefined){showBook(Number(element.dataset.roleHelp));return;}
  if(element.dataset.pickRole!==undefined){showRolePicker(Number(element.dataset.pickRole));return;}
  if(element.dataset.rolePick!==undefined){const s=Number(element.dataset.roleSlot);roles[s]=Number(element.dataset.rolePick);slot=s;piece=0;formations[s]=[cells[s]||0,0,0];dialog.close();render();return;}
  if(element.dataset.slot!==undefined){slot=Number(element.dataset.slot);piece=0;render();return;}
  if(element.dataset.piece!==undefined){piece=Number(element.dataset.piece);render();return;}
  if(element.dataset.unit!==undefined){const id=Number(element.dataset.unit);if(currentView().budget.active>=0&&currentView().budget.active!==id){message('本回合已使用另一名角色。');return;}unit=id;kind=-1;target=null;variant=0;render();return;}
  if(element.dataset.kind!==undefined){if(element.closest('#dialog'))dialog.close();kind=Number(element.dataset.kind);variant=0;target=null;render();return;}
  if(element.dataset.target!==undefined){target=Number(element.dataset.target);render();return;}
  if(element.dataset.cell!==undefined){
    const c=Number(element.dataset.cell);
    if(state.phase==='setup'){
      if(state.setup)return;if(![0,3].includes(state.map.cells[c-1])){message('这里不能站立，请选择空地或漩涡。');return;}
      if(roles[slot]===10){formations[slot][piece]=c;if(piece===0)cells[slot]=c;if(piece<2)piece++;}
      else{cells[slot]=c;if(slot+1<state.config.slots){slot++;piece=0;}}
      render();return;
    }
    if(historyView)return;
    if(immediate.has(kind))return;
    if(!currentActions().some(a=>a[2]===c)){message('该格不是当前动作的合法目标。');return;}
    target=c;render();return;
  }
  const command=element.dataset.command;if(!command)return;event.preventDefault();
  if(command==='close-dialog'){dialog.close();return;}
  if(command==='book'){showBook();return;}
  if(command==='room-menu'){showRoomMenu();return;}
  if(command==='unit-info'){showUnitInfo();return;}
  if(command==='all-actions'){showActions();return;}
  if(command==='log'){if(state?.view){dialog.close();battleLog.open();}return;}
  if(command==='rules'){openDialog(`${dialogHeader('在迷雾中对决')}${instructions()}`);dialog.querySelector('details').open=true;return;}
  if(command==='home'){if(!state){history.replaceState(null,'','/');render();}else message('请使用“离开房间”，以免丢失当前席位。');return;}
  if(command==='toggle-animation'){visuals.toggle();if(dialog.open)showRoomMenu();return;}
  if(command==='animation-speed'){visuals.cycleSpeed();if(dialog.open)showRoomMenu();return;}
  if(command==='skip-playback'){visuals.skip();return;}
  if(command==='live'){historyView=null;kind=-1;target=null;render();return;}
  if(command==='invite'){
    const url=`${location.origin}/?room=${state.code}`;
    openDialog(`${dialogHeader('邀请好友')}<p class="preline">房间号：${state.code}\n把下方邀请链接发给好友。电脑和手机打开后，点击“加入房间”即可；使用公网网址时，可从不同网络加入。</p><input class="invite-input" aria-label="邀请地址" readonly value="${esc(url)}"><p class="muted" style="margin-top:12px">点击地址后全选复制，或在手机上长按复制。</p>`);dialog.querySelector('input').addEventListener('click',e=>e.target.select());return;
  }
  perform(async()=>{
    if(command==='ready'){const answer=await api(endpoint('ready'),state.setup?{ready:false}:{roles:roles.slice(0,state.config.slots),cells:cells.slice(0,state.config.slots),formations:formations.slice(0,state.config.slots)});receive(answer.state);}
    if(command==='execute'){const choices=currentActions();const a=immediate.has(kind)?choices[0]:choices.find(a=>a[2]===target);if(a)await sendAction(a);}
    if(command==='end'){const a=unitActions().find(a=>a[1]===35);if(a)await sendAction(a);}
    if(command==='surrender'&&confirm('确定投降吗？这将淘汰你在本局的所有角色。')){const a=state.view.actions.find(a=>a[1]===26);if(a)await sendAction(a);}
    if(command==='rematch'){const answer=await api(endpoint('rematch'),{});historyView=null;receive(answer.state);}
    if(command==='leave'&&confirm(state.you===0?'离开会关闭本房间。确定离开？':'确定离开这个房间？')){await api(endpoint('leave'),{});clearSession();history.replaceState(null,'','/');}
  });
}
document.addEventListener('click',clicked);
document.addEventListener('change',event=>{
  if(event.target.id==='variant'){variant=Number(event.target.value);target=null;render();}
});
dialog.addEventListener('click',event=>{if(event.target===dialog)dialog.close();});
document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible'&&session)connect();});
window.addEventListener('online',()=>{if(session)connect();});
window.addEventListener('pagehide',()=>{streamGeneration++;streamController?.abort();});
async function boot(){
  try{
    catalog=await api('/api/catalog',undefined,false);
    if(session){try{const answer=await api(endpoint('state'));receive(answer.state);connect();return;}catch(error){if(error.status===401||error.status===404){session=null;remember();}else throw error;}}
    render();
  }catch(error){app.innerHTML=`<main class="loading"><img src="/mark.svg" width="64" height="64" alt=""><h1>暂时无法连接森林</h1><p class="muted">服务可能正在启动或网络暂时中断，请稍后重新连接。</p><button class="btn primary" data-command="reload">重新连接</button></main>`;}
}
document.addEventListener('click',event=>{if(event.target.closest('[data-command="reload"]'))location.reload();});
for(const eventName of ['pointerover','focusin'])app.addEventListener(eventName,event=>{
  const cell=event.target.closest('[data-cell]'),output=app.querySelector('.board-cursor');
  if(cell&&output)output.textContent=cell.getAttribute('aria-label')+(cell.classList.contains('attacked')?' · 敌方公开攻击痕迹，不表示站位':'')+(cell.classList.contains('legal')?' · 当前可选目标':'');
});
boot();
