param([string]$BackupId,[string]$SandboxRoot)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
$paths=Get-DriverPaths $SandboxRoot
Assert-VrStopped $paths
$backupRoot=Join-Path $PSScriptRoot 'Backups'
if (!$BackupId) { $BackupId=(Get-Content -LiteralPath (Join-Path $backupRoot 'latest.txt') -Raw).Trim() }
if ($BackupId -notmatch '^\d{8}-\d{6}-[0-9a-fA-F-]{36}$') { throw 'Invalid backup identifier.' }
$backup=Assert-ChildPath $backupRoot (Join-Path $backupRoot $BackupId)
$state=Get-Content -LiteralPath (Join-Path $backup 'state.json') -Raw | ConvertFrom-Json
foreach ($pair in @(@('runtime','Runtime'),@('config','Config'),@('steamConfig','SteamConfig'))) {
    if ([IO.Path]::GetFullPath($state.($pair[0])) -ne [IO.Path]::GetFullPath($paths[$pair[1]])) { throw 'Backup belongs to a different SteamVR/config installation.' }
}
# Validate every required backup before touching any destination.
foreach ($entry in @(@('mainExisted','previous-driver'),@('privateExisted','previous-private'),@('settingsExisted','settings.json'),@('steamSettingsExisted','steamvr.vrsettings'))) {
    if ($state.($entry[0]) -and !(Test-Path -LiteralPath (Join-Path $backup $entry[1]))) { throw "Incomplete backup: $($entry[1])" }
}
$drivers=Join-Path $paths.Runtime 'drivers'
foreach ($entry in @(@('CustomHeadsetOpenVR','mainExisted','previous-driver'),@('CustomHeadsetOpenVR_Private','privateExisted','previous-private'))) {
    $target=Assert-ChildPath $drivers (Join-Path $drivers $entry[0])
    Remove-DriverDirectory $drivers $target
    if ($state.($entry[1])) { Copy-Item -LiteralPath (Join-Path $backup $entry[2]) -Destination $target -Recurse }
}
foreach ($entry in @(@($paths.Config,'settings.json','settingsExisted'),@($paths.SteamConfig,'steamvr.vrsettings','steamSettingsExisted'))) {
    $target=Assert-ChildPath $entry[0] (Join-Path $entry[0] $entry[1])
    if ($state.($entry[2])) { Copy-Item -LiteralPath (Join-Path $backup $entry[1]) -Destination $target -Force }
    elseif (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
}
Write-Host "Restored the exact pre-install driver and settings from $BackupId."
