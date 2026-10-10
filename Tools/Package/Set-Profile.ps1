param([ValidateSet('Guard','Jitter','Smooth','Baseline','Pimax')][string]$Profile='Guard',[string]$SandboxRoot)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
$paths=Get-DriverPaths $SandboxRoot
if (!$paths.Sandbox -and (Get-Process -Name custom-headset-gui -ErrorAction SilentlyContinue)) { throw 'Close the GUI first so it cannot overwrite the profile change.' }
$file=Join-Path $paths.Config 'settings.json'
$settings=Read-JsonOrEmpty $file
if($Profile -eq 'Pimax' -and $settings.PSObject.Properties['onlyHandlePrivateFunctionality'] -and $settings.onlyHandlePrivateFunctionality){throw 'Install this release with EVO OpenPort before selecting native Pimax tracking.'}
$nativeWasSelected=$settings.PSObject.Properties['dreamAir'] -and $settings.dreamAir.PSObject.Properties['useNativePimaxTracking'] -and $settings.dreamAir.useNativePimaxTracking
if($Profile -eq 'Pimax' -or $nativeWasSelected){Assert-VrStopped $paths}
$backupRoot=Join-Path $PSScriptRoot 'ProfileBackups'
[IO.Directory]::CreateDirectory($backupRoot) | Out-Null
if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination (Join-Path $backupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString()+'.json')) }
Write-JsonAtomic $file (Apply-Profile $settings $Profile)
Write-Host "Profile set to $Profile."
if($Profile -eq 'Pimax'){Write-Host 'Pimax native headset tracking uses the existing floor origin and room, with no Lighthouse HMD alignment or automatic recenter. Start SteamVR in OpenPort mode.'}
elseif($nativeWasSelected){Write-Host 'Start SteamVR to use the Lighthouse headset path.'}
else{Write-Host 'The fork reloads this Lighthouse filtering profile automatically.'}
Write-Host 'Existing force-tracking settings were preserved.'
