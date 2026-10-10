param(
    [ValidateSet('Play','OpenPort')][string]$Backend='Play',
    [ValidateSet('Guard','Jitter','Smooth','Baseline','Pimax')][string]$Profile='Guard',
    [string]$SandboxRoot
)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
$paths=Get-DriverPaths $SandboxRoot
Assert-VrStopped $paths
$drivers=Join-Path $paths.Runtime 'drivers'
if (!(Test-Path -LiteralPath $drivers)) { throw "SteamVR drivers directory not found: $drivers" }
$main=Assert-ChildPath $drivers (Join-Path $drivers 'CustomHeadsetOpenVR')
$private=Assert-ChildPath $drivers (Join-Path $drivers 'CustomHeadsetOpenVR_Private')
$settingsPath=Join-Path $paths.Config 'settings.json'
$steamPath=Join-Path $paths.SteamConfig 'steamvr.vrsettings'
$settings=Read-JsonOrEmpty $settingsPath
$steamSettings=Read-JsonOrEmpty $steamPath
$settings=Apply-Profile $settings $Profile
Set-JsonProperty $settings 'onlyHandlePrivateFunctionality' ($Backend -eq 'Play')
foreach ($name in @('driver_CustomHeadsetOpenVR','driver_CustomHeadsetOpenVR_Private')) {
    $value=$steamSettings.PSObject.Properties[$name]
    $entry=if ($value) { $value.Value } else { [PSCustomObject]@{} }
    Set-JsonProperty $entry 'enable' ($name -eq 'driver_CustomHeadsetOpenVR' -or $Backend -eq 'Play')
    Set-JsonProperty $entry 'blocked_by_safe_mode' $false
    Set-JsonProperty $steamSettings $name $entry
}
$sourceMain=Join-Path $PSScriptRoot 'CustomHeadsetOpenVR'
$sourcePrivate=Join-Path $PSScriptRoot 'CustomHeadsetOpenVR_Private'
if (!(Test-Path -LiteralPath (Join-Path $sourceMain 'bin\win64\driver_CustomHeadsetOpenVR.dll'))) { throw 'Compiled driver is missing from package.' }
if ($Backend -eq 'Play' -and !(Test-Path -LiteralPath (Join-Path $sourcePrivate 'bin\win64\driver_CustomHeadsetOpenVR_Private.dll'))) { throw 'Upstream private helper is missing from package.' }
$backupRoot=Join-Path $PSScriptRoot 'Backups'
$backup=Join-Path $backupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString())
[IO.Directory]::CreateDirectory($backup) | Out-Null
$state=[PSCustomObject]@{ version=1; runtime=$paths.Runtime; config=$paths.Config; steamConfig=$paths.SteamConfig; mainExisted=(Test-Path -LiteralPath $main); privateExisted=(Test-Path -LiteralPath $private); settingsExisted=(Test-Path -LiteralPath $settingsPath); steamSettingsExisted=(Test-Path -LiteralPath $steamPath); backend=$Backend; profile=$Profile }
if ($state.mainExisted) { Copy-DriverDirectory $main (Join-Path $backup 'previous-driver') }
if ($state.privateExisted) { Copy-DriverDirectory $private (Join-Path $backup 'previous-private') }
if ($state.settingsExisted) { Copy-Item -LiteralPath $settingsPath -Destination (Join-Path $backup 'settings.json') }
if ($state.steamSettingsExisted) { Copy-Item -LiteralPath $steamPath -Destination (Join-Path $backup 'steamvr.vrsettings') }
Write-JsonAtomic (Join-Path $backup 'state.json') $state
$stageMain=Assert-ChildPath $drivers (Join-Path $drivers ('.lh1-main-'+[guid]::NewGuid().ToString()))
$stagePrivate=Assert-ChildPath $drivers (Join-Path $drivers ('.lh1-private-'+[guid]::NewGuid().ToString()))
$mutating=$false
try {
    Copy-DriverDirectory $sourceMain $stageMain
    if ($Backend -eq 'Play') { Copy-DriverDirectory $sourcePrivate $stagePrivate }
    Assert-VrStopped $paths
    $mutating=$true
    Remove-DriverDirectory $drivers $main
    Move-Item -LiteralPath $stageMain -Destination $main
    Remove-DriverDirectory $drivers $private
    if ($Backend -eq 'Play') { Move-Item -LiteralPath $stagePrivate -Destination $private }
    Write-JsonAtomic $settingsPath $settings
    Write-JsonAtomic $steamPath $steamSettings
    [IO.File]::WriteAllText((Join-Path $backupRoot 'latest.txt'),(Split-Path -Leaf $backup))
    Write-Host "Installed experimental Dream Air LH1 fork. Backend: $Backend; profile: $Profile."
    Write-Host "Backup: $backup"
    Write-Host 'Start Pimax and SteamVR normally. Use Collect-Diagnostics.cmd to verify the new build and tracking counters.'
} catch {
    $failure=$_
    if ($mutating) {
        try { & (Join-Path $PSScriptRoot 'Rollback.ps1') -BackupId (Split-Path -Leaf $backup) -SandboxRoot $SandboxRoot }
        catch { Write-Warning "Automatic rollback failed. Preserve backup $backup. Error: $_" }
    }
    throw $failure
} finally {
    Remove-DriverDirectory $drivers $stageMain
    Remove-DriverDirectory $drivers $stagePrivate
}
