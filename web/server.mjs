import http from 'node:http';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {readFile,readdir,stat} from 'node:fs/promises';
import {createReadStream} from 'node:fs';
import {randomBytes,randomInt} from 'node:crypto';
import {networkInterfaces} from 'node:os';
import {fileURLToPath} from 'node:url';
import path from 'node:path';

const root=fileURLToPath(new URL('../',import.meta.url));
const binary=path.join(root,'build-web',process.platform==='win32'?'referee.exe':'referee');
const port=Number(process.env.PORT||8787),host=process.env.HOST||'0.0.0.0';
const rooms=new Map(),connections=new Set(),buckets=new Map();
const ROOM_LIMIT=Number(process.env.MAX_ROOMS||8),IDLE_MS=2*60*60*1000;
const fail=(message,status=400)=>Object.assign(new Error(message),{status});
const integer=(value,min,max)=>Number.isInteger(value)&&value>=min&&value<=max;
const nameOf=value=>typeof value==='string'?value.trim().replace(/[\u0000-\u001f\u007f]/g,'').slice(0,16)||'旅人':'旅人';

class Referee {
  constructor() {
    this.pending=[];this.dead=false;
    this.process=spawn(binary,[],{cwd:root,stdio:['pipe','pipe','pipe'],windowsHide:true});
    createInterface({input:this.process.stdout}).on('line',line=>{
      const entry=this.pending.shift();if(!entry)return;
      clearTimeout(entry.timer);
      try {entry.resolve(JSON.parse(line));}catch {entry.reject(fail('裁判响应异常。',503));}
    });
    this.process.stderr.on('data',()=>{});
    this.process.on('error',()=>this.stop('无法启动游戏裁判，请先构建。'));
    this.process.on('exit',()=>this.stop('游戏裁判已停止。'));
  }
  command(line) {
    if(this.dead)return Promise.reject(fail('游戏裁判已停止。',503));
    return new Promise((resolve,reject)=>{
      const entry={resolve,reject,timer:setTimeout(()=>this.stop('裁判响应超时。'),15000)};
      this.pending.push(entry);
      this.process.stdin.write(`${line}\n`,error=>{if(error)this.stop('游戏裁判连接中断。');});
    });
  }
  stop(message='房间已关闭。') {
    if(this.dead)return;this.dead=true;
    this.process.kill();
    for(const p of this.pending){clearTimeout(p.timer);p.reject(fail(message,503));}
    this.pending=[];
  }
}
const metadata=new Referee();
const catalog=await metadata.command('catalog');
if(!catalog.ok)throw Error('无法读取职业规则。');
const maps=[];
for(const [id,label,code] of [['default','默认森林','default'],['empty','全空地图','empty']]) {
  const answer=await metadata.command(`map ${code}`);maps.push({id,name:label,...answer.map});
}
for(const filename of await readdir(path.join(root,'maps'))) {
  if(!filename.endsWith('.mistmap'))continue;
  const text=await readFile(path.join(root,'maps',filename),'utf8');
  const match=text.trim().match(/^MIST_FOREST_MAP\s+1\s+(\d+)$/);
  if(!match)continue;
  const answer=await metadata.command(`map ${match[1]}`);
  if(answer.ok&&!maps.some(m=>m.encoded===answer.map.encoded))maps.push({id:filename,name:filename.replace('.mistmap',''),...answer.map});
}
metadata.stop();
catalog.maps=maps;

function serialize(room,work) {
  const task=room.queue.then(work);
  room.queue=task.catch(()=>{});
  return task;
}
function roomFor(code) {
  const room=rooms.get(code);
  if(!room)throw fail('房间不存在或已过期，请重新创建。',404);
  return room;
}
function authenticate(req,room) {
  const token=req.headers.authorization?.match(/^Bearer ([a-f0-9]{48})$/)?.[1];
  const seat=room.seats.find(s=>s?.token===token);
  if(!seat)throw fail('席位已失效，请重新加入房间。',401);
  room.touched=Date.now();
  return seat;
}
function newSeat(room,slot,nickname) {
  return {slot,name:nameOf(nickname),token:randomBytes(24).toString('hex'),setup:null,streams:new Set(),seen:new Map()};
}
function info(room,seat) {
  return {code:room.code,phase:room.phase,epoch:room.epoch,sequence:room.sequence,host:0,you:seat.slot,
    config:room.config,map:room.map,error:room.error,
    seats:room.seats.map((s,i)=>s?{slot:i,name:s.name,ready:!!s.setup,online:s.streams.size>0,
      roles:s.setup?.roles||[]}:null),
    // Never serialize another seat's deployment, bearer token, or request IDs.
    setup:seat.setup,revision:room.views?.[seat.slot]?.revision??0,view:room.views?.[seat.slot]||null};
}
function writeEvent(res,value) {
  if(res.destroyed||res.writableEnded)return;
  // Do not buffer an unlimited event queue for a backgrounded or slow browser.
  if(res.writableLength>512*1024){res.destroy();return;}
  res.write(`data: ${JSON.stringify(value)}\n\n`);
}
function broadcast(room) {
  room.sequence++;
  for(const seat of room.seats)if(seat)for(const res of seat.streams)writeEvent(res,info(room,seat));
}
async function updateViews(room) {
  room.views=[];
  for(let p=0;p<room.config.players;p++) {
    const answer=await room.referee.command(`view ${p}`);
    if(!answer.ok)throw fail(answer.error,503);
    room.views.push(answer.view);
  }
  room.phase=room.views[0].winner===-1?'playing':'finished';
}
async function startMatch(room) {
  const positions=Array(8).fill(0),roles=Array(8).fill(0),formations=Array(24).fill(0);
  for(const seat of room.seats)for(let k=0;k<room.config.slots;k++) {
    const id=seat.slot*2+k;positions[id]=seat.setup.cells[k];roles[id]=seat.setup.roles[k];
    for(let piece=0;piece<3;piece++)formations[id*3+piece]=seat.setup.formations[k]?.[piece]||0;
  }
  room.referee=new Referee();
  const pairs=positions.flatMap((pos,i)=>[pos,roles[i]]).join(' ');
  const result=await room.referee.command(`new ${room.config.players} ${room.config.slots} ${+room.config.teams} ${randomInt(room.config.players)} ${room.map.encoded} ${pairs} ${formations.join(' ')}`);
  if(!result.ok){room.referee.stop();room.referee=null;for(const s of room.seats)s.setup=null;broadcast(room);throw fail(result.error);}
  await updateViews(room);
}
function validateSetup(room,data) {
  const count=room.config.slots,total=room.map.width*room.map.height;
  if(!Array.isArray(data.roles)||data.roles.length!==count||data.roles.some(r=>!integer(r,0,12))||new Set(data.roles).size!==count)
    throw fail('每个角色槽选择一个不同的职业。');
  if(!Array.isArray(data.cells)||data.cells.length!==count||data.cells.some(c=>!integer(c,1,total)||![0,3].includes(room.map.cells[c-1])))
    throw fail('请选择可站立的格子作为起点。');
  const formations=Array.from({length:count},()=>[0,0,0]);
  for(let k=0;k<count;k++)if(data.roles[k]===10) {
    const cells=data.formations?.[k];
    if(!Array.isArray(cells)||cells.length!==3||cells[0]!==data.cells[k]||new Set(cells).size!==3||
      cells.some(c=>!integer(c,1,total)||![0,3].includes(room.map.cells[c-1])))
      throw fail('守卫军需部署三名士兵，首名士兵与角色起点相同，三格不能重复。');
    formations[k]=cells;
  }
  return {roles:data.roles,cells:data.cells,formations};
}
async function body(req) {
  let size=0;const chunks=[];
  for await(const chunk of req){size+=chunk.length;if(size>16384)throw fail('请求过大。',413);chunks.push(chunk);}
  try{return JSON.parse(Buffer.concat(chunks).toString('utf8'));}catch{throw fail('请求格式不正确。');}
}
function json(res,status,value) {
  res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});res.end(JSON.stringify(value));
}
function limit(req) {
  const key=req.socket.remoteAddress,now=Date.now();
  let bucket=buckets.get(key);
  if(!bucket||now-bucket.at>10000){bucket={at:now,count:0};buckets.set(key,bucket);}
  if(++bucket.count>120)throw fail('操作过于频繁，请稍后再试。',429);
}
const types={'.html':'text/html; charset=utf-8','.mjs':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.png':'image/png','.svg':'image/svg+xml','.ico':'image/x-icon'};
async function staticFile(res,url) {
  const asset=url.pathname.startsWith('/assets/');
  const base=path.join(root,asset?'src/assets':'web/public');
  const relative=decodeURIComponent(asset?url.pathname.slice(8):url.pathname==='/'?'index.html':url.pathname.slice(1));
  const file=path.resolve(base,relative);
  if(!file.startsWith(base+path.sep)||!types[path.extname(file)]||relative.includes('\\'))throw fail('文件不存在。',404);
  const st=await stat(file).catch(()=>null);if(!st?.isFile())throw fail('文件不存在。',404);
  res.writeHead(200,{'Content-Type':types[path.extname(file)],'Content-Length':st.size,'Cache-Control':asset?'public, max-age=86400':'no-cache'});
  const stream=createReadStream(file);stream.on('error',()=>res.destroy());stream.pipe(res);
}
const server=http.createServer(async(req,res)=>{
  res.setHeader('X-Content-Type-Options','nosniff');
  res.setHeader('Referrer-Policy','no-referrer');
  res.setHeader('Content-Security-Policy',"default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'");
  try {
    const url=new URL(req.url,'http://local');
    if(!url.pathname.startsWith('/api/')){if(req.method!=='GET')throw fail('方法不支持。',405);await staticFile(res,url);return;}
    limit(req);
    if(req.headers.origin&&new URL(req.headers.origin).host!==req.headers.host)throw fail('请从游戏页面执行操作。',403);
    if(req.method==='GET'&&url.pathname==='/api/health'){json(res,200,{ok:true,version:catalog.version,rooms:rooms.size});return;}
    if(req.method==='GET'&&url.pathname==='/api/catalog'){json(res,200,catalog);return;}
    if(req.method==='POST'&&url.pathname==='/api/rooms') {
      if(rooms.size>=ROOM_LIMIT)throw fail('房间已满，请等待其他对局结束。',503);
      const data=await body(req),players=data.players??2,slots=data.slots??2;
      if(!integer(players,2,4)||!integer(slots,1,2)||data.teams&&players!==4)throw fail('房规配置无效。');
      const map=maps.find(m=>m.id===data.map)||maps[0];
      let code;do{code=randomBytes(4).toString('hex').slice(0,6).toUpperCase();}while(rooms.has(code));
      const room={code,phase:'setup',epoch:1,sequence:0,config:{players,slots,teams:!!data.teams},map,
        seats:Array(players).fill(null),referee:null,views:null,queue:Promise.resolve(),touched:Date.now(),error:''};
      room.seats[0]=newSeat(room,0,data.name);rooms.set(code,room);
      json(res,201,{ok:true,code,token:room.seats[0].token,state:info(room,room.seats[0])});return;
    }
    const match=url.pathname.match(/^\/api\/rooms\/([A-F0-9]{6})(?:\/(join|events|state|ready|action|rematch|history|leave))?$/);
    if(!match)throw fail('接口不存在。',404);
    const room=roomFor(match[1]),operation=match[2]||'state';
    if(operation==='join'&&req.method==='POST') {
      const data=await body(req);
      await serialize(room,async()=>{
        const slot=room.seats.findIndex(s=>s===null);
        if(room.phase!=='setup'||slot===-1)throw fail('房间已满或已经开始。');
        const seat=newSeat(room,slot,data.name);room.seats[slot]=seat;room.touched=Date.now();broadcast(room);
        json(res,200,{ok:true,code:room.code,token:seat.token,state:info(room,seat)});
      });return;
    }
    const seat=authenticate(req,room);
    if(operation==='events'&&req.method==='GET') {
      if(seat.streams.size>=3)for(const previous of seat.streams){previous.end();seat.streams.delete(previous);}
      res.writeHead(200,{'Content-Type':'text/event-stream; charset=utf-8','Cache-Control':'no-cache, no-transform','Connection':'keep-alive','X-Accel-Buffering':'no'});
      res.flushHeaders();seat.streams.add(res);broadcast(room);
      const heartbeat=setInterval(()=>{if(res.writableLength>512*1024)res.destroy();else res.write(': ping\n\n');},12000);
      res.on('close',()=>{clearInterval(heartbeat);seat.streams.delete(res);if(rooms.has(room.code))broadcast(room);});return;
    }
    if(operation==='state'&&req.method==='GET'){json(res,200,{ok:true,state:info(room,seat)});return;}
    if(operation==='history'&&req.method==='GET') {
      const step=Number(url.searchParams.get('step'));
      await serialize(room,async()=>{
        if(!room.referee||!integer(step,0,room.views[seat.slot].revision))throw fail('历史步骤无效。');
        const answer=await room.referee.command(`history ${seat.slot} ${step}`);json(res,answer.ok?200:400,answer);
      });return;
    }
    if(req.method!=='POST')throw fail('方法不支持。',405);
    const data=await body(req);
    await serialize(room,async()=>{
      if(!room.seats.includes(seat))throw fail('席位已失效。',401);
      if(operation==='ready') {
        if(room.phase!=='setup')throw fail('对局已经开始。');
        seat.setup=data.ready===false?null:validateSetup(room,data);
        if(room.seats.every(s=>s?.setup))await startMatch(room);
        broadcast(room);json(res,200,{ok:true,state:info(room,seat)});return;
      }
      if(operation==='action') {
        if(typeof data.id!=='string'||!data.id.match(/^[\w-]{8,80}$/))throw fail('动作编号无效。');
        const signature=JSON.stringify([data.epoch,data.revision,data.action]);
        if(seat.seen.has(data.id)) {
          if(seat.seen.get(data.id)!==signature)throw fail('动作编号已使用。',409);
          json(res,200,{ok:true,duplicate:true,state:info(room,seat)});return;
        }
        if(room.phase!=='playing')throw fail('对局尚未开始或已结束。');
        if(data.epoch!==room.epoch)throw fail('对局已更换，请重新选择动作。',409);
        const action=data.action;
        if(!integer(data.revision,0,100000)||!Array.isArray(action)||action.length!==8||action.some(n=>!integer(n,-1,100)))throw fail('动作格式无效。');
        const live=room.views[seat.slot];
        if(data.revision!==live.revision)throw fail('局面已更新，请重新选择动作。',409);
        if(!live.actions.some(candidate=>candidate.every((n,i)=>n===action[i])))throw fail('该动作不属于你当前可执行的动作。',409);
        if(room.views[seat.slot].revision>5000&&action[1]!==26)throw fail('本局步骤达到上限，请投降后重新开局。');
        const answer=await room.referee.command(`act ${seat.slot} ${data.revision} ${action.join(' ')}`);
        if(!answer.ok)throw fail(answer.error,409);
        seat.seen.set(data.id,signature);if(seat.seen.size>128)seat.seen.delete(seat.seen.keys().next().value);
        await updateViews(room);broadcast(room);json(res,200,{ok:true,state:info(room,seat)});return;
      }
      if(operation==='rematch') {
        if(seat.slot!==0||room.phase!=='finished')throw fail('对局结束后由房主开始下一局。');
        room.referee.stop();room.referee=null;room.views=null;room.phase='setup';room.epoch++;
        for(const s of room.seats)if(s){s.setup=null;s.seen.clear();}
        broadcast(room);json(res,200,{ok:true,state:info(room,seat)});return;
      }
      if(operation==='leave') {
        if(room.phase==='playing')throw fail('请先完成对局或在自己的回合投降。');
        if(seat.slot===0) {
          room.referee?.stop();rooms.delete(room.code);
          for(const s of room.seats)if(s)for(const stream of s.streams){writeEvent(stream,{closed:true});stream.end();}
        } else {room.seats[seat.slot]=null;for(const stream of seat.streams)stream.end();broadcast(room);}
        json(res,200,{ok:true});return;
      }
      throw fail('接口不存在。',404);
    });
  } catch(error) {
    if(!res.headersSent)json(res,error.status||500,{ok:false,error:error.status?error.message:'服务器处理失败，请重连后再试。'});
    else res.end();
  }
});
server.on('connection',socket=>{connections.add(socket);socket.on('close',()=>connections.delete(socket));});
const cleanup=setInterval(()=>{
  for(const [code,room] of rooms)if(Date.now()-room.touched>IDLE_MS&&!room.seats.some(s=>s?.streams.size)) {room.referee?.stop();rooms.delete(code);}
  for(const [key,bucket] of buckets)if(Date.now()-bucket.at>60000)buckets.delete(key);
},60000);
function shutdown(){clearInterval(cleanup);for(const room of rooms.values())room.referee?.stop();for(const socket of connections)socket.destroy();server.close();}
process.once('SIGINT',shutdown);process.once('SIGTERM',shutdown);
server.on('error',error=>{console.error(`启动失败：${error.message}`);shutdown();process.exitCode=1;});
server.listen(port,host,()=>{
  console.log(`雾隐之森网页版 ${catalog.version}\n电脑访问：http://127.0.0.1:${port}`);
  if(host==='0.0.0.0')for(const [name,addresses] of Object.entries(networkInterfaces()))for(const ip of addresses||[])if(ip.family==='IPv4'&&!ip.internal)console.log(`候选局域网地址（${name}）：http://${ip.address}:${port}`);
  console.log('手机请选择与 Wi-Fi 同网段的地址；VPN/虚拟网卡地址不一定可用。首次访问请允许专用网络防火墙提示。');
  console.log('保持此窗口开启；Ctrl+C 停止服务。房间在服务停止后清空。');
});
