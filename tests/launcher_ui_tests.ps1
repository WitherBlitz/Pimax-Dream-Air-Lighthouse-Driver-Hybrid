param([string]$TestRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(!$TestRoot){$TestRoot=Join-Path $repo ('output\Tests\LauncherUI-'+[guid]::NewGuid().ToString('N'))}
$TestRoot=[IO.Path]::GetFullPath($TestRoot)
if(Test-Path -LiteralPath $TestRoot){throw 'Use a fresh UI test directory.'}
foreach($directory in @('CustomHeadset\Distortion','SteamVR\drivers','SteamConfig')){[IO.Directory]::CreateDirectory((Join-Path $TestRoot $directory)) | Out-Null}
foreach($marker in @('.launcher-ui-sandbox','.dreamair-lh1-sandbox')){[IO.File]::WriteAllText((Join-Path $TestRoot $marker),'isolated UI test')}
Copy-Item -LiteralPath (Join-Path $repo 'Tools\Package\DriverTools.ps1'),(Join-Path $repo 'Tools\Package\DistortionTools.ps1'),(Join-Path $repo 'Tools\Package\Set-Distortion.ps1') -Destination $TestRoot
Copy-Item -LiteralPath (Join-Path $repo 'Tools\Package\Distortion') -Destination $TestRoot -Recurse
[IO.File]::WriteAllText((Join-Path $TestRoot 'Install.ps1'),'# UI sandbox placeholder; never run')
[IO.File]::WriteAllText((Join-Path $TestRoot 'Floor-Height.ps1'),"param([switch]`$Read)`nWrite-Host 'LAUNCHER-SMOKE-READ-ONLY'`n")
[IO.File]::WriteAllText((Join-Path $TestRoot 'CustomHeadset\info.json'),'{"builtInDistortionProfiles":{"Dream Air Default":{"device":["DreamAir","Other"],"type":"RadialBezier","distortions":[0,0,55,117]},"Dream Air like Pimax":{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,115]}}}')
$settings=Join-Path $TestRoot 'CustomHeadset\settings.json'
$before='{"dreamAir":{"distortionProfile":"Dream Air Default","ipd":64.1,"eyeRotation":0,"useNativePimaxTracking":true},"forceTracking":true,"dreamAirPoseSource":"Pimax","room":{"floor":1.905,"universe":31}}'
[IO.File]::WriteAllText($settings,$before)
$exe=Join-Path $repo 'output\Tools\DreamAir-LH1.exe'
foreach($mode in @('--smoke-test','--ui-smoke-test')){
    $preview=Join-Path $TestRoot $(if($mode -eq '--smoke-test'){'opening.png'}else{'applied.png'})
    $process=Start-Process -FilePath $exe -ArgumentList @($mode,('"{0}"' -f $TestRoot),('"{0}"' -f $preview)) -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -or !(Test-Path -LiteralPath $preview)){throw "Launcher UI test failed: $mode. See $preview.error.txt"}
    if($mode -eq '--smoke-test' -and [IO.File]::ReadAllText($settings) -cne $before){throw 'Opening the launcher rewrote settings.'}
}
$actual=Get-Content -LiteralPath $settings -Raw | ConvertFrom-Json
if($actual.dreamAir.distortionProfile -cne 'Dream Air Maximum Comfort 100pct Overlap' -or $actual.dreamAir.ipd -ne 64.1 -or $actual.dreamAir.eyeRotation -ne 0 -or !$actual.dreamAir.useNativePimaxTracking -or !$actual.forceTracking -or $actual.dreamAirPoseSource -cne 'Pimax' -or $actual.room.floor -ne 1.905 -or $actual.room.universe -ne 31){throw 'UI Apply failed to select custom or changed unrelated settings.'}
$default=Join-Path $TestRoot 'CustomHeadset\Distortion\Dream Air Maximum Comfort 100pct Overlap.json'
if((Get-FileHash -LiteralPath $default).Hash -cne '00B17F0D11166C0796F344EB3F414FE113E898CBDA04E9C40A94C05B0FC8806E'){throw 'UI Apply did not copy exact bundled profile bytes.'}
Write-Host 'PASS launcher opening is read-only, current builtin is shown, and UI Apply selects exact custom profile while preserving settings'
Write-Host "UI previews: $TestRoot"
