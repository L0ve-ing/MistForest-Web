const escape=value=>String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

// A separate dialog keeps live board renders from replacing the reader's scroll
// position, filter, focus or selection. It consumes only the seat's filtered View.
export class BattleLog {
  constructor(dialog,{onRead,onHistory,onLive,onClose}) {
    Object.assign(this,{dialog,onRead,onHistory,onLive,onClose});
    this.filter='all';this.seen=new Set();this.entries=[];this.signature='';this.atBottom=true;
    dialog.addEventListener('click',event=>{
      const button=event.target.closest('button');
      if(button?.dataset.logFilter){this.filter=button.dataset.logFilter;this.paint(true);}
      if(button?.dataset.logCommand==='close')dialog.close();
      if(button?.dataset.logCommand==='latest')this.toLatest();
      if(button?.dataset.logCommand==='live')this.onLive();
      if(event.target===dialog)dialog.close();
    });
    dialog.addEventListener('change',event=>{
      if(event.target.id==='history-step')this.onHistory(Number(event.target.value));
    });
    dialog.addEventListener('close',()=>this.onClose?.());
    document.addEventListener('visibilitychange',()=>{if(!document.hidden&&dialog.open)this.markRead();});
  }
  update({view,key,revision,historical,busy}) {
    if(key!==this.key){
      this.key=key;this.filter='all';this.signature='';this.atBottom=true;this.opened=false;
      try{this.seen=new Set(JSON.parse(sessionStorage.getItem('mistforest-log-'+key)||'[]'));}catch{this.seen=new Set();}
    }
    Object.assign(this,{view,revision,historical,busy});
    const occurrences=new Map();
    this.entries=(view?.notices||[]).map(notice=>{
      const base=JSON.stringify([notice.revision,!!notice.feedback,notice.text]);
      const count=occurrences.get(base)||0;occurrences.set(base,count+1);
      return {...notice,key:base+':'+count};
    });
    const signature=JSON.stringify([this.entries,view?.turnReport,view?.units?.map(u=>u.report),revision,historical,busy]);
    if(this.dialog.open&&signature!==this.signature)this.paint();
    this.signature=signature;
  }
  unread(){return this.entries.filter(entry=>!this.seen.has(entry.key)).length;}
  open(){
    this.dialog.showModal();this.paint(!this.opened);this.opened=true;
    this.dialog.querySelector('[data-log-command="close"]').focus({preventScroll:true});
  }
  selected(){return this.entries.filter(n=>this.filter==='all'||(this.filter==='private'?n.feedback:!n.feedback));}
  markRead(){
    if(!this.dialog.open||!this.atBottom||document.hidden)return;
    const entries=this.selected();let changed=false;
    for(const entry of entries)if(!this.seen.has(entry.key)){this.seen.add(entry.key);changed=true;}
    if(!changed)return;
    this.seen=new Set([...this.seen].slice(-256));
    try{sessionStorage.setItem('mistforest-log-'+this.key,JSON.stringify([...this.seen]));}catch{}
    this.onRead?.();this.status();
  }
  status(){
    const unread=this.unread();
    const badge=this.dialog.querySelector('.log-unread');if(badge)badge.textContent=unread?`${unread} 条未读`:'已读到最新';
    const jump=this.dialog.querySelector('[data-log-command="latest"]');
    if(jump){jump.hidden=this.atBottom;jump.textContent=unread?`${unread} 条新战报 · 查看最新`:'回到最新';}
  }
  toLatest(){const list=this.dialog.querySelector('.log-scroll');if(!list)return;list.scrollTop=list.scrollHeight;this.atBottom=true;this.markRead();this.status();}
  paint(reset=false){
    const previous=this.dialog.querySelector('.log-scroll'),scrollTop=previous?.scrollTop||0;
    const stick=reset||this.atBottom;
    const focused=document.activeElement?.dataset.logFilter;
    const focusedCommand=document.activeElement?.dataset.logCommand;
    const expanded=this.dialog.querySelector('.log-intel')?.open;
    const intelScrollTop=this.dialog.querySelector('.log-intel')?.scrollTop||0;
    const historyExpanded=this.dialog.querySelector('.log-history')?.open;
    const entries=this.selected(),groups=new Map();
    for(const n of entries){if(!groups.has(n.revision))groups.set(n.revision,[]);groups.get(n.revision).push(n);}
    const ownReports=(this.view?.units||[]).filter(u=>u.report).map(u=>`<p>${escape(u.report)}</p>`).join('');
    const intel=this.view?.turnReport||ownReports;
    this.dialog.innerHTML=`<div class="log-shell">
      <header class="log-header"><div><h2 id="battle-log-title">战报</h2><span class="log-unread"></span></div><button class="quiet" data-log-command="close">返回棋盘</button></header>
      <nav class="log-filters" aria-label="筛选战报">${[['all','全部'],['public','公开'],['private','仅自己']].map(([key,label])=>`<button data-log-filter="${key}" aria-pressed="${this.filter===key}" class="${this.filter===key?'active':''}">${label}</button>`).join('')}</nav>
      ${this.filter!=='public'&&intel?`<details class="log-intel" ${expanded?'open':''}><summary>本回合私人情报</summary>${this.view.turnReport?`<p>${escape(this.view.turnReport)}</p>`:''}${ownReports}</details>`:''}
      <div class="log-scroll" tabindex="0" aria-label="战报记录">
        ${[...groups].map(([step,notices])=>`<section class="log-step"><h3>第 ${step} 步</h3><ol>${notices.map(n=>`<li class="${n.feedback?'private-notice ':''}${n.weight>1?'important ':''}${this.seen.has(n.key)?'':'unread-notice'}"><span class="notice-audience">${n.feedback?'仅你可见':'公开'}</span><p>${escape(n.text)}</p></li>`).join('')}</ol></section>`).join('')||`<p class="log-empty">${this.filter==='private'?'暂无私人反馈。':this.filter==='public'?'暂无公开行动。':'尚未产生战报。'}</p>`}
        ${this.entries.length>=100?'<p class="log-limit">显示最近 100 条记录；更早局面可在下方回顾。</p>':''}
      </div>
      <footer class="log-footer"><button class="quiet jump-latest" data-log-command="latest" hidden>回到最新</button>
        <details class="log-history" ${historyExpanded?'open':''}><summary>回顾自己的历史视角${this.historical!==null?` · 第 ${this.historical} 步`:''}</summary><div class="history-bar"><label for="history-step">选择步骤</label><input id="history-step" type="range" min="0" max="${this.revision||0}" value="${this.historical??this.revision??0}" ${this.busy?'disabled':''}><span>${this.historical??this.revision??0} 步</span><button class="quiet" data-log-command="live">回到当前</button></div></details>
      </footer></div>`;
    const list=this.dialog.querySelector('.log-scroll');
    const intelPanel=this.dialog.querySelector('.log-intel');
    if(intelPanel){intelPanel.scrollTop=intelScrollTop;intelPanel.addEventListener('toggle',()=>{if(this.atBottom)this.toLatest();});}
    list.scrollTop=stick?list.scrollHeight:scrollTop;
    this.atBottom=stick||list.scrollHeight-list.scrollTop-list.clientHeight<28;
    list.addEventListener('scroll',()=>{this.atBottom=list.scrollHeight-list.scrollTop-list.clientHeight<28;this.markRead();this.status();},{passive:true});
    if(focused)this.dialog.querySelector(`[data-log-filter="${focused}"]`)?.focus({preventScroll:true});
    else if(focusedCommand)this.dialog.querySelector(`[data-log-command="${focusedCommand}"]`)?.focus({preventScroll:true});
    this.markRead();this.status();
  }
}
