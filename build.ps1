param([string]$OutputDirectory=(Join-Path $PSScriptRoot 'build'))
$ErrorActionPreference='Stop'
$sourceRoot=Join-Path $PSScriptRoot 'src'
$objectRoot=Join-Path $OutputDirectory 'obj'
[IO.Directory]::CreateDirectory($objectRoot) | Out-Null
$compiler=(Get-Command g++.exe -ErrorAction Stop).Source
$resourceCompiler=(Get-Command windres.exe -ErrorAction Stop).Source
$flags=@('-std=c++17','-O2','-finput-charset=UTF-8','-static','-static-libgcc','-static-libstdc++')
$sources=[ordered]@{
    'damage'='damage.cpp';'game'='game.cpp';'network'='network.cpp'
    'action_selection'='action_selection.cpp';'tutorial'='tutorial/guide.cpp'
    'playback'='presentation/playback.cpp';'skill_fx'='presentation/skill_fx.cpp'
    'ui_commands'='ui/command_catalog.cpp';'map_editor'='editor/map_editor.cpp'
}
$objects=@()
foreach($source in $sources.GetEnumerator()) {
    $object=Join-Path $objectRoot ($source.Key+'.o')
    & $compiler @flags '-Wall' '-Wextra' '-Werror' '-pedantic' '-c' (Join-Path $sourceRoot $source.Value) '-o' $object
    if($LASTEXITCODE -ne 0) {throw "Compilation failed: $($source.Value)"}
    $objects+=$object
}
$resource=Join-Path $objectRoot 'version.o'
& $resourceCompiler '-c' '65001' '-I' $sourceRoot (Join-Path $sourceRoot 'version.rc') '-O' 'coff' '-o' $resource
if($LASTEXITCODE -ne 0) {throw 'Resource compilation failed.'}
$program=Join-Path $OutputDirectory 'MistForest.exe'
& $compiler @flags '-municode' '-mwindows' (Join-Path $sourceRoot 'app.cpp') @objects $resource '-lws2_32' '-lcomdlg32' '-lgdi32' '-luser32' '-lgdiplus' '-lmsimg32' '-lole32' '-pthread' '-o' $program
if($LASTEXITCODE -ne 0) {throw 'Application build failed.'}
$mapDirectory=Join-Path $OutputDirectory 'maps'
[IO.Directory]::CreateDirectory($mapDirectory) | Out-Null
Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'maps') -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $mapDirectory -Force
}
Write-Output "Built: $program"
Get-FileHash -LiteralPath $program -Algorithm SHA256
