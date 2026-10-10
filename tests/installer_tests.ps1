param([Parameter(Mandatory=$true)][string]$Package,[Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
. (Join-Path $Package 'DriverTools.ps1')
$testBase=[IO.Path]::GetFullPath($TestRoot)
[IO.Directory]::CreateDirectory($testBase) | Out-Null
function Check($ok,$message) { if (!$ok) { throw $message } }
function New-Sandbox([string]$name) {
    $root=Join-Path $testBase ($name+'-'+[guid]::NewGuid().ToString())
    [IO.Directory]::CreateDirectory((Join-Path $root 'SteamVR\drivers')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $root 'CustomHeadset\Distortion')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $root 'SteamConfig')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $root '.dreamair-lh1-sandbox'),'test')
    return $root
}
function File-Hash([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
$cases=0
$root=New-Sandbox 'existing'
$cfg=Join-Path $root 'CustomHeadset\settings.json'
$steam=Join-Path $root 'SteamConfig\steamvr.vrsettings'
[IO.File]::WriteAllText($cfg,'{ "dreamAir": { "ipd": 64.1, "distortionProfile": "custom", "enableEyeTracking": true }, "unknownUserValue": { "x": 3 }, "forceTracking": true }')
[IO.File]::WriteAllText($steam,'{ "driver_CustomHeadsetOpenVR": { "enable": false, "hasBeenRun": "older" }, "other": {"thing":5} }')
[IO.Directory]::CreateDirectory((Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR_Private')) | Out-Null
[IO.File]::WriteAllText((Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR\original.txt'),'original main')
[IO.File]::WriteAllText((Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR_Private\original.txt'),'original private')
[IO.File]::WriteAllText((Join-Path $root 'CustomHeadset\Distortion\custom.json'),'do not touch')
$cfgHash=File-Hash $cfg; $steamHash=File-Hash $steam
& (Join-Path $Package 'Install.ps1') -SandboxRoot $root -Backend Play
$settings=Read-JsonOrEmpty $cfg
Check ($settings.forceTracking -and $settings.dreamAirTracking.enabled -and !$settings.dreamAirTracking.smoothPosition) 'Default guard or force preservation incorrect.'
Check ($settings.dreamAir.ipd -eq 64.1 -and $settings.dreamAir.enableEyeTracking -and $settings.unknownUserValue.x -eq 3) 'User settings lost.'
Check ($settings.onlyHandlePrivateFunctionality) 'Play helper not enabled.'
Check ((Get-Content -LiteralPath (Join-Path $root 'CustomHeadset\Distortion\custom.json') -Raw) -eq 'do not touch') 'Distortion modified.'
$cases++; Write-Host 'PASS Play install preserves settings and distortion'
& (Join-Path $Package 'Set-Profile.ps1') -SandboxRoot $root -Profile Jitter
Check ((Read-JsonOrEmpty $cfg).dreamAirTracking.smoothPosition) 'Jitter profile not applied.'
& (Join-Path $Package 'Set-Profile.ps1') -SandboxRoot $root -Profile Baseline
$settings=Read-JsonOrEmpty $cfg
Check (!$settings.dreamAirTracking.enabled -and $settings.forceTracking) 'Baseline or force preservation incorrect.'
$cases++; Write-Host 'PASS profile changes preserve explicit forced tracking'
$privateHash=File-Hash $cfg
$rejected=$false
try { & (Join-Path $Package 'Set-Profile.ps1') -SandboxRoot $root -Profile Pimax } catch { $rejected=$true }
Check ($rejected -and (File-Hash $cfg) -eq $privateHash) 'Private-only Pimax profile was not rejected before changing settings.'
$rejected=$false
try { & (Join-Path $Package 'Install.ps1') -SandboxRoot $root -Backend Play -Profile Pimax } catch { $rejected=$true }
Check ($rejected -and (File-Hash $cfg) -eq $privateHash) 'Play/Pimax install was not rejected before changing settings.'
$cases++; Write-Host 'PASS Play/Pimax mismatch rejected without changing settings'
& (Join-Path $Package 'Rollback.ps1') -SandboxRoot $root
& (Join-Path $Package 'Install.ps1') -SandboxRoot $root -Backend OpenPort -Profile Pimax
$settings=Read-JsonOrEmpty $cfg
Check ($settings.dreamAirPoseSource -eq 'Pimax' -and $settings.dreamAir.useNativePimaxTracking -and !$settings.dreamAirTracking.enabled -and $settings.forceTracking) 'Pimax profile or force preservation incorrect.'
$cases++; Write-Host 'PASS Pimax profile selects native source without changing force flags'
& (Join-Path $Package 'Rollback.ps1') -SandboxRoot $root
Check ((File-Hash $cfg) -eq $cfgHash -and (File-Hash $steam) -eq $steamHash) 'Rollback was not byte exact.'
Check (Test-Path -LiteralPath (Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR\original.txt')) 'Main driver not restored.'
Check (Test-Path -LiteralPath (Join-Path $root 'SteamVR\drivers\CustomHeadsetOpenVR_Private\original.txt')) 'Prior helper not restored.'
$cases++; Write-Host 'PASS rollback restores both old drivers and exact settings bytes'
$empty=New-Sandbox 'empty'
& (Join-Path $Package 'Install.ps1') -SandboxRoot $empty -Backend OpenPort -Profile Pimax
Check (!(Test-Path -LiteralPath (Join-Path $empty 'SteamVR\drivers\CustomHeadsetOpenVR_Private'))) 'OpenPort installed private helper.'
Check (!(Read-JsonOrEmpty (Join-Path $empty 'CustomHeadset\settings.json')).onlyHandlePrivateFunctionality) 'OpenPort private mode incorrect.'
Check ((Read-JsonOrEmpty (Join-Path $empty 'CustomHeadset\settings.json')).dreamAirPoseSource -eq 'Pimax') 'Install did not select native Pimax source.'
& (Join-Path $Package 'Rollback.ps1') -SandboxRoot $empty
Check (!(Test-Path -LiteralPath (Join-Path $empty 'CustomHeadset\settings.json'))) 'Originally absent settings survived rollback.'
Check (!(Test-Path -LiteralPath (Join-Path $empty 'SteamConfig\steamvr.vrsettings'))) 'Originally absent SteamVR settings survived rollback.'
Check (!(Test-Path -LiteralPath (Join-Path $empty 'SteamVR\drivers\CustomHeadsetOpenVR'))) 'Originally absent driver survived rollback.'
$cases++; Write-Host 'PASS OpenPort install/rollback on clean state'
$malformed=New-Sandbox 'malformed'
[IO.File]::WriteAllText((Join-Path $malformed 'CustomHeadset\settings.json'),'{ broken')
$rejected=$false
try { & (Join-Path $Package 'Install.ps1') -SandboxRoot $malformed } catch { $rejected=$true }
Check $rejected 'Malformed settings accepted.'
Check (!(Test-Path -LiteralPath (Join-Path $malformed 'SteamVR\drivers\CustomHeadsetOpenVR'))) 'Malformed install changed drivers.'
$cases++; Write-Host 'PASS malformed settings rejected before mutation'
$rejected=$false
try { Assert-ChildPath (Join-Path $root 'SteamVR\drivers') (Join-Path $root 'outside') | Out-Null } catch { $rejected=$true }
Check $rejected 'Unsafe path accepted.'
$cases++; Write-Host 'PASS out-of-root destructive targets rejected'
Write-Host "$cases/8 installer cases passed"
