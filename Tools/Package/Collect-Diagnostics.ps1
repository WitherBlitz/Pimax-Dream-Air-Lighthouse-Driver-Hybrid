param([ValidateRange(1,1800)][int]$Seconds=30,[string]$OutputDirectory)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
$paths=Get-DriverPaths ''
if (!$OutputDirectory) { $OutputDirectory=Join-Path $PSScriptRoot ('Diagnostics\'+(Get-Date -Format 'yyyyMMdd-HHmmss')) }
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$file=Join-Path $paths.Config 'settings.json'
if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination (Join-Path $OutputDirectory 'settings.json') }
Write-Host "Recording $Seconds seconds. Put the headset still for a jitter baseline, or reproduce the problem for a failure trace."
& (Join-Path $PSScriptRoot 'Tools\TrackingRecorder.exe') --seconds $Seconds --output (Join-Path $OutputDirectory 'poses.csv') | Tee-Object -FilePath (Join-Path $OutputDirectory 'devices-and-summary.txt')
$recorderExit=$LASTEXITCODE
foreach ($name in @('dreamair-tracking.json','diagnostic.json')) {
    $path=Join-Path $paths.Config $name
    if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $OutputDirectory $name) }
}
$vrpaths=Get-Content -LiteralPath (Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath') -Raw | ConvertFrom-Json
$log=Join-Path $vrpaths.log[0] 'vrserver.txt'
if (Test-Path -LiteralPath $log) {
    Copy-Item -LiteralPath $log -Destination (Join-Path $OutputDirectory 'vrserver.txt')
    $patterns='axis .*appears to have failed|Resetting tracking|no optical samples|bootstrap pose|Dropped .*hits|Moving base|DreamAir LH1 stability|PoseUpdated from driver'
    Select-String -LiteralPath $log -Pattern $patterns | Select-Object -Last 120 | ForEach-Object { $_.Line } | Set-Content -LiteralPath (Join-Path $OutputDirectory 'tracking-log-excerpts.txt')
}
Write-Host "Saved diagnostics locally to $OutputDirectory"
if ($recorderExit -ne 0) { throw "Recorder exited with $recorderExit; available logs were still collected." }
