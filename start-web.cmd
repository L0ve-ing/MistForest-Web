@echo off
cd /d "%~dp0"
if exist "runtime\node.exe" (
  "runtime\node.exe" "scripts\start-web.mjs"
) else (
  node "scripts\start-web.mjs"
)
pause
