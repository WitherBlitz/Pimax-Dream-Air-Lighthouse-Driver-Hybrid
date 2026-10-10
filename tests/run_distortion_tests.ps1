param([string]$TestRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(!$TestRoot){$TestRoot=Join-Path (Split-Path -Parent $repo) ('distortion-tests-'+[guid]::NewGuid().ToString('N').Substring(0,8))}
$TestRoot=[IO.Path]::GetFullPath($TestRoot)
if(Test-Path -LiteralPath $TestRoot){throw 'Use a fresh distortion test directory.'}
$package=Join-Path $TestRoot 'package'
[IO.Directory]::CreateDirectory($package) | Out-Null
Copy-Item -Path (Join-Path $repo 'Tools\Package\*') -Destination $package -Recurse
foreach($name in @('CustomHeadsetOpenVR','CustomHeadsetOpenVR_Private')){
    $bin=Join-Path $package "$name\bin\win64";[IO.Directory]::CreateDirectory($bin) | Out-Null
    $dll=if($name -eq 'CustomHeadsetOpenVR'){'driver_CustomHeadsetOpenVR.dll'}else{'driver_CustomHeadsetOpenVR_Private.dll'}
    [IO.File]::WriteAllText((Join-Path $bin $dll),'isolated-test-driver')
}
& (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'distortion_tests.ps1') -Package $package -TestRoot (Join-Path $TestRoot 'sandboxes')
if($LASTEXITCODE){throw 'Distortion sandbox tests failed.'}
Write-Host "Distortion test artifacts: $TestRoot"
