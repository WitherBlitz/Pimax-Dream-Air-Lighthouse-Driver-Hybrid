param([string]$VisualStudioPath,[string]$Toolset='v145')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if (!$VisualStudioPath) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
if (!$VisualStudioPath) { throw 'Visual Studio C++ tools were not found.' }
$msbuild=Join-Path $VisualStudioPath 'MSBuild\Current\Bin\MSBuild.exe'
$vcvars=Join-Path $VisualStudioPath 'VC\Auxiliary\Build\vcvars64.bat'
Push-Location $repo
try {
    & $msbuild 'CustomHeadsetOpenVR\CustomHeadsetOpenVR.vcxproj' /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$Toolset" /p:SkipPostBuild=true "/p:SolutionDir=$repo\" /m /v:minimal
    if ($LASTEXITCODE) { throw "Driver build failed: $LASTEXITCODE" }
    [IO.Directory]::CreateDirectory((Join-Path $repo 'output\Tools')) | Out-Null
    $compile='"'+$vcvars+'" && cl /nologo /EHsc /std:c++17 /W4 /wd4100 /MT /I ThirdParty\openvr\headers tests\pose_stability_tests.cpp /Fe:output\Tools\PoseTests.exe /Fo:output\Tools\PoseTests.obj && output\Tools\PoseTests.exe && cl /nologo /EHsc /std:c++17 /W4 /wd4100 /MT /I ThirdParty\openvr\headers Tools\TrackingRecorder.cpp ThirdParty\openvr\lib\win64\openvr_api.lib /Fe:output\Tools\TrackingRecorder.exe /Fo:output\Tools\TrackingRecorder.obj'
    & cmd.exe /d /c $compile
    if ($LASTEXITCODE) { throw "Tests or recorder build failed: $LASTEXITCODE" }
    $floorCompile='"'+$vcvars+'" && cl /nologo /EHsc /std:c++17 /W4 /wd4100 /MT /I ThirdParty\openvr\headers /I ThirdParty\json\include tests\floor_height_tests.cpp /Fe:output\Tools\FloorTests.exe /Fo:output\Tools\FloorTests.obj && output\Tools\FloorTests.exe && cl /nologo /EHsc /std:c++17 /W4 /wd4100 /MT /I ThirdParty\openvr\headers /I ThirdParty\json\include Tools\FloorHeight.cpp ThirdParty\openvr\lib\win64\openvr_api.lib /Fe:output\Tools\FloorHeight.exe /Fo:output\Tools\FloorHeight.obj'
    & cmd.exe /d /c $floorCompile
    if ($LASTEXITCODE) { throw "Floor tests or tool build failed: $LASTEXITCODE" }
    Copy-Item -LiteralPath (Join-Path $repo 'ThirdParty\openvr\bin\win64\openvr_api.dll') -Destination (Join-Path $repo 'output\Tools\openvr_api.dll') -Force
} finally { Pop-Location }
