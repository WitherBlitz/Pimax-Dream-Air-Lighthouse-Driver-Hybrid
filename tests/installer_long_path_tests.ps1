param([Parameter(Mandatory=$true)][string]$Package,[Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
# Exercise the real installer in a marked sandbox. The old recursive Copy-Item
# loses resources once a backup filename crosses MAX_PATH on PowerShell 5.1.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class LongPathFixture {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool CopyFile(string source, string destination, bool failIfExists);
}
'@
function Check($ok,$message) { if (!$ok) { throw $message } }
function Hash([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Check-LongFile([string]$path,[string]$expected,[string]$scratch) {
    $readback=Join-Path $scratch ([guid]::NewGuid().ToString('N')+'.bin')
    if (![LongPathFixture]::CopyFile(('\\?\'+$path),$readback,$true)) { throw ('Backup file missing/unreadable: '+$path+'; Win32 '+[Runtime.InteropServices.Marshal]::GetLastWin32Error()) }
    Check ((Hash $readback) -eq $expected) 'Long backup file did not preserve its exact bytes.'
}
$base=[IO.Path]::GetFullPath((Join-Path $TestRoot ([guid]::NewGuid().ToString('N').Substring(0,8))))
[IO.Directory]::CreateDirectory($base) | Out-Null
$leaf='resources\icons\crystal\headset_status_ready_alert_2x.b4bfb144.png'
$cases=0; $failures=0
foreach ($backend in @('Play','OpenPort')) {
    $case=Join-Path $base $backend
    [IO.Directory]::CreateDirectory($case) | Out-Null
    # Match the nested extraction in the report; keep the backup root itself
    # short enough for state.json while its nested resource exceeds 260 chars.
    $padding='release & [fixture] '+('x'*(150-$case.Length-20))
    $pkg=Join-Path $case $padding
    [IO.Directory]::CreateDirectory($pkg) | Out-Null
    foreach ($script in @('DriverTools.ps1','Install.ps1','Rollback.ps1','Set-Profile.ps1')) { Copy-Item -LiteralPath (Join-Path $Package $script) -Destination (Join-Path $pkg $script) }
    foreach ($name in @('CustomHeadsetOpenVR','CustomHeadsetOpenVR_Private')) {
        $dll=if ($name -eq 'CustomHeadsetOpenVR') { 'driver_CustomHeadsetOpenVR.dll' } else { 'driver_CustomHeadsetOpenVR_Private.dll' }
        $folder=Join-Path $pkg ($name+'\bin\win64')
        [IO.Directory]::CreateDirectory($folder) | Out-Null
        [IO.File]::WriteAllText((Join-Path $folder $dll),'new test binary '+$backend)
        [IO.File]::WriteAllText((Join-Path $pkg ($name+'\driver.vrdrivermanifest')),'{"name":"test"}')
    }
    $sandbox=Join-Path $case 'sandbox'
    [IO.Directory]::CreateDirectory((Join-Path $sandbox 'CustomHeadset')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $sandbox 'SteamConfig')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $sandbox '.dreamair-lh1-sandbox'),'test')
    $cfg=Join-Path $sandbox 'CustomHeadset\settings.json'
    $steam=Join-Path $sandbox 'SteamConfig\steamvr.vrsettings'
    [IO.File]::WriteAllText($cfg,'{ "keep": "user value", "forceTracking": true }')
    [IO.File]::WriteAllText($steam,'{ "keep": "steam value" }')
    $cfgHash=Hash $cfg; $steamHash=Hash $steam
    $main=Join-Path $sandbox 'SteamVR\drivers\CustomHeadsetOpenVR'
    $private=Join-Path $sandbox 'SteamVR\drivers\CustomHeadsetOpenVR_Private'
    foreach ($driver in @($main,$private)) {
        [IO.Directory]::CreateDirectory((Join-Path $driver 'resources\icons\crystal')) | Out-Null
        [IO.Directory]::CreateDirectory((Join-Path $driver 'empty-directory')) | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $driver $leaf),[byte[]](0,1,2,255,7,8,9))
        [IO.File]::WriteAllText((Join-Path $driver 'original.txt'),'original '+$backend)
    }
    $resourceHash=Hash (Join-Path $main $leaf)
    try {
        & (Join-Path $pkg 'Install.ps1') -Backend $backend -SandboxRoot $sandbox
        $id=(Get-Content -LiteralPath (Join-Path $pkg 'Backups\latest.txt') -Raw).Trim()
        $backup=Join-Path $pkg ('Backups\'+$id)
        foreach ($old in @('previous-driver','previous-private')) {
            $longResource=Join-Path $backup ($old+'\'+$leaf)
            Check ($longResource.Length -gt 260) 'Fixture did not exceed MAX_PATH.'
            Check-LongFile $longResource $resourceHash $case
            Check (Test-Path -LiteralPath (Join-Path $backup ($old+'\empty-directory'))) 'Empty directory lost during backup.'
        }
        Check (Test-Path -LiteralPath (Join-Path $main 'bin\win64\driver_CustomHeadsetOpenVR.dll')) 'Driver staging did not complete.'
        & (Join-Path $pkg 'Rollback.ps1') -SandboxRoot $sandbox
        foreach ($driver in @($main,$private)) {
            Check ((Hash (Join-Path $driver $leaf)) -eq $resourceHash) 'Restore lost or changed a nested resource.'
            Check (Test-Path -LiteralPath (Join-Path $driver 'empty-directory')) 'Restore lost an empty directory.'
            Check (!(Test-Path -LiteralPath (Join-Path $driver 'bin\win64'))) 'Restore retained new driver files.'
        }
        Check ((Hash $cfg) -eq $cfgHash -and (Hash $steam) -eq $steamHash) 'Restore changed settings bytes.'
        $cases++; Write-Host "PASS $backend long-path backup/install/restore preserves bytes and empty directories"
    } catch { $failures++; Write-Host "FAIL $backend long-path lifecycle: $_"; Write-Host $_.ScriptStackTrace }
    # A real copy error must not be mistaken for Robocopy's success code 1.
    # Hold an original resource exclusively so its backup cannot be read.
    try {
        $pointer=Join-Path $pkg 'Backups\latest.txt'
        $priorPointer=Hash $pointer
        $resource=Join-Path $main $leaf
        $locked=[IO.File]::Open($resource,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        $rejected=$false
        try { & (Join-Path $pkg 'Install.ps1') -Backend $backend -SandboxRoot $sandbox }
        catch { $rejected=$true }
        finally { $locked.Dispose() }
        Check $rejected 'Unreadable backup source was accepted.'
        Check ((Hash $pointer) -eq $priorPointer) 'Failed backup replaced the previous restore pointer.'
        Check ((Hash $cfg) -eq $cfgHash -and (Hash $steam) -eq $steamHash) 'Failed backup changed live settings.'
        foreach ($driver in @($main,$private)) {
            Check ((Hash (Join-Path $driver $leaf)) -eq $resourceHash) 'Failed backup changed the original driver.'
            Check (!(Test-Path -LiteralPath (Join-Path $driver 'bin\win64'))) 'Failed backup installed the new driver.'
        }
        $cases++; Write-Host "PASS $backend copy failure leaves driver/settings/restore pointer unchanged"
    } catch { $failures++; Write-Host "FAIL $backend backup failure guard: $_"; Write-Host $_.ScriptStackTrace }
}
if ($failures) { throw "$cases/4 long-path installer cases passed; $failures failed." }
Write-Host "$cases/4 long-path installer cases passed (isolated fake drivers; no live mutations)"
