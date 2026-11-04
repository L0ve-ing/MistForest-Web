import {spawn} from 'node:child_process';
import {mkdir,access} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root=fileURLToPath(new URL('../',import.meta.url));
const bin=path.join(root,'build-web');
await mkdir(bin,{recursive:true});
const target=path.join(bin,process.platform==='win32'?'referee.exe':'referee');
const args=['-std=c++17','-O2','-Wall','-Wextra','-Werror','-pedantic','-finput-charset=UTF-8',
  'web/referee.cpp','src/game.cpp','src/damage.cpp','src/presentation/skill_fx.cpp','-o',target];
if(process.platform==='win32') args.push('-static','-static-libgcc','-static-libstdc++');
let compiler=process.env.CXX||'g++';
if(process.platform==='win32'&&!process.env.CXX) {
  try {await access('C:/msys64/ucrt64/bin/g++.exe');compiler='C:/msys64/ucrt64/bin/g++.exe';}catch{}
}
const child=spawn(compiler,args,{cwd:root,stdio:'inherit',windowsHide:true});
child.on('error',()=>{console.error('找不到 C++ 编译器。请安装 MinGW（Windows）或 g++（Linux），也可直接使用运行包。');process.exitCode=1;});
child.on('exit',code=>{if(code!==0)process.exitCode=1;else console.log(`Built: ${target}`);});
