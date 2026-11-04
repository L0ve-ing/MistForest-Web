import {access} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {spawn} from 'node:child_process';
const root=new URL('../',import.meta.url);
try {await access(new URL(`build-web/referee${process.platform==='win32'?'.exe':''}`,root));}
catch {
  const code=await new Promise((resolve,reject)=>{
    const child=spawn(process.execPath,[fileURLToPath(new URL('scripts/build-web.mjs',root))],{stdio:'inherit',windowsHide:true});
    child.on('error',reject);child.on('exit',resolve);
  });
  if(code!==0) process.exit(1);
}
await import('../web/server.mjs');
