$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'The Windows .NET Framework C# compiler is unavailable.' }
Push-Location $repo
try {
    [IO.Directory]::CreateDirectory((Join-Path $repo 'output\Tools')) | Out-Null
    & $compiler /nologo /optimize+ /warn:4 /target:exe /platform:x64 /r:System.Web.Extensions.dll /out:output\Tools\LauncherTests.exe Tools\DistortionCatalog.cs Tools\LauncherModel.cs Tools\LauncherRunner.cs tests\launcher_tests.cs
    if ($LASTEXITCODE) { throw 'Launcher tests did not compile.' }
    & .\output\Tools\LauncherTests.exe
    if ($LASTEXITCODE) { throw 'Launcher command tests failed.' }
    & $compiler /nologo /optimize+ /warn:4 /target:winexe /platform:x64 /win32manifest:Tools\Launcher.manifest /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /out:output\Tools\DreamAir-LH1.exe Tools\DistortionCatalog.cs Tools\LauncherModel.cs Tools\LauncherRunner.cs Tools\Launcher.cs
    if ($LASTEXITCODE) { throw 'Launcher build failed.' }
    & (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher_host_tests.ps1
    if ($LASTEXITCODE) { throw 'Launcher elevation-host tests failed.' }
    & (Join-Path $repo 'tests\run_distortion_tests.ps1')
} finally { Pop-Location }
