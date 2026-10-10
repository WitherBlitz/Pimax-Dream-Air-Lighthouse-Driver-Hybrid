$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$sandbox=Join-Path (Split-Path -Parent $repo) ('launcher-host-tests\'+[guid]::NewGuid().ToString())
[IO.Directory]::CreateDirectory($sandbox) | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'Tools\Package\Launcher-Elevated.ps1') -Destination $sandbox
[IO.File]::WriteAllText((Join-Path $sandbox 'Install.ps1'),@'
param([string]$Backend,[string]$Profile)
Write-Host ('TEST install '+$Backend+' profile '+$Profile)
'@)
[IO.File]::WriteAllText((Join-Path $sandbox 'Rollback.ps1'),"throw 'TEST rollback failed'")
$shell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$hostScript=Join-Path $sandbox 'Launcher-Elevated.ps1'
$log=Join-Path $sandbox 'success.txt'
& $shell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $hostScript -Operation Install-OpenPort -LogPath $log -Profile Pimax
if ($LASTEXITCODE -ne 0 -or !(Get-Content -LiteralPath $log -Raw).Contains('TEST install OpenPort profile Pimax')) { throw 'Success profile/log/exit code missing.' }
Write-Host 'PASS elevation host forwards backend and selected profile and captures output'
$failureLog=Join-Path $sandbox 'failure.txt'
& $shell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $hostScript -Operation Rollback -LogPath $failureLog
if ($LASTEXITCODE -ne 1 -or !(Get-Content -LiteralPath $failureLog -Raw).Contains('TEST rollback failed')) { throw 'Failure log/exit code missing.' }
Write-Host 'PASS elevation host propagates failure and retains its log'
$badLog=Join-Path $sandbox 'invalid.txt'
$priorErrorAction=$ErrorActionPreference
try {
    $ErrorActionPreference='Continue'
    & $shell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $hostScript -Operation Unknown -LogPath $badLog *> (Join-Path $sandbox 'invalid-error.txt')
    $invalidExit=$LASTEXITCODE
} finally { $ErrorActionPreference=$priorErrorAction }
if ($invalidExit -eq 0 -or (Test-Path -LiteralPath $badLog)) { throw 'Unknown action accepted.' }
Write-Host 'PASS elevation host refuses unrecognized action'
Write-Host '3/3 host cases passed (isolated fake scripts; no live install or rollback)'
exit 0
