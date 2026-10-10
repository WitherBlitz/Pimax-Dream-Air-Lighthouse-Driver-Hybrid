param([ValidateSet('Guard','Jitter','Smooth','Baseline')][string]$Profile='Guard',[string]$SandboxRoot)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
$paths=Get-DriverPaths $SandboxRoot
if (!$paths.Sandbox -and (Get-Process -Name custom-headset-gui -ErrorAction SilentlyContinue)) { throw 'Close the GUI first so it cannot overwrite the profile change.' }
$file=Join-Path $paths.Config 'settings.json'
$settings=Read-JsonOrEmpty $file
$backupRoot=Join-Path $PSScriptRoot 'ProfileBackups'
[IO.Directory]::CreateDirectory($backupRoot) | Out-Null
if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination (Join-Path $backupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString()+'.json')) }
Write-JsonAtomic $file (Apply-Profile $settings $Profile)
Write-Host "Profile set to $Profile. The fork reloads it automatically; no VR restart is required."
