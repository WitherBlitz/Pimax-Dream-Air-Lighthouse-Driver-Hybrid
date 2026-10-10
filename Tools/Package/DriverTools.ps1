Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'

function Get-DriverPaths([string]$SandboxRoot) {
    if ($SandboxRoot) {
        $sandbox = [IO.Path]::GetFullPath($SandboxRoot)
        if (!(Test-Path -LiteralPath (Join-Path $sandbox '.dreamair-lh1-sandbox'))) { throw 'Sandbox marker is missing.' }
        return @{ Runtime=(Join-Path $sandbox 'SteamVR'); Config=(Join-Path $sandbox 'CustomHeadset'); SteamConfig=(Join-Path $sandbox 'SteamConfig'); Sandbox=$true }
    }
    $vrpaths = Get-Content -LiteralPath (Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath') -Raw | ConvertFrom-Json
    if (!$vrpaths.runtime -or !$vrpaths.config) { throw 'SteamVR runtime/config paths were not found.' }
    return @{ Runtime=[IO.Path]::GetFullPath($vrpaths.runtime[0]); Config=(Join-Path $env:APPDATA 'CustomHeadset'); SteamConfig=[IO.Path]::GetFullPath($vrpaths.config[0]); Sandbox=$false }
}
function Assert-VrStopped($paths) {
    if ($paths.Sandbox) { return }
    $running = @(Get-Process -Name vrserver,vrcompositor,custom-headset-gui -ErrorAction SilentlyContinue)
    if ($running.Count) { throw 'Close SteamVR and the CustomHeadset GUI first. No processes were stopped and no live settings were changed.' }
}
function Assert-ChildPath([string]$Parent,[string]$Child) {
    $root=[IO.Path]::GetFullPath($Parent).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $full=[IO.Path]::GetFullPath($Child)
    if (!$full.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw "Path is outside the intended directory: $full" }
    $cursor=$full
    while ($cursor -and $cursor.Length -ge $root.TrimEnd('\','/').Length) {
        if (Test-Path -LiteralPath $cursor) {
            $item=Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to modify a linked path: $cursor" }
        }
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}
function Remove-DriverDirectory([string]$Parent,[string]$Child) {
    $target=Assert-ChildPath $Parent $Child
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
}
function Copy-DriverDirectory([string]$Source,[string]$Destination) {
    $sourcePath=[IO.Path]::GetFullPath($Source)
    $destinationPath=[IO.Path]::GetFullPath($Destination)
    if (!(Test-Path -LiteralPath $sourcePath -PathType Container)) { throw "Driver copy source is missing: $sourcePath" }
    # Windows PowerShell 5.1 Copy-Item fails on nested backup resources above
    # MAX_PATH. Robocopy supports these paths without changing machine policy.
    # Copy only: never mirror, purge or move. Preserve empty directories and
    # copy links themselves rather than following them outside the tree.
    $copyOptions=@('/E','/COPY:DAT','/DCOPY:DAT','/SL','/SJ','/R:0','/W:0','/NP','/NFL','/NDL','/NJH','/NJS')
    try {
        $report=@(& (Join-Path $env:WINDIR 'System32\robocopy.exe') $sourcePath $destinationPath @copyOptions 2>&1)
        $copyExit=$LASTEXITCODE
    } finally {
        # Robocopy's successful "files copied" code is 1, not a script failure.
        $global:LASTEXITCODE=0
    }
    if ($copyExit -ge 8) { throw "Driver directory copy failed (robocopy exit $copyExit): $sourcePath -> $destinationPath`n$($report -join [Environment]::NewLine)" }
    if (!(Test-Path -LiteralPath $destinationPath -PathType Container)) { throw "Driver copy destination is missing: $destinationPath" }
}
function Set-JsonProperty($Object,[string]$Name,$Value) {
    $Object | Add-Member -MemberType NoteProperty -Name $Name -Value $Value -Force
}
function Read-JsonOrEmpty([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        $result=Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        if ($null -eq $result -or $result -is [Array] -or $result -is [string] -or $result -is [ValueType]) { throw "Expected a JSON object: $Path" }
        return $result
    }
    return [PSCustomObject]@{}
}
function Get-NativeFilePath([string]$Path) {
    $full=[IO.Path]::GetFullPath($Path)
    if ($full.StartsWith('\\?\')) { return $full }
    if ($full.StartsWith('\\')) { return '\\?\UNC\'+$full.Substring(2) }
    return '\\?\'+$full
}
function Write-JsonAtomic([string]$Path,$Object) {
    $parent=Split-Path -Parent $Path
    [IO.Directory]::CreateDirectory((Get-NativeFilePath $parent)) | Out-Null
    $temp=Join-Path $parent ('.lh1-json-'+[guid]::NewGuid().ToString()+'.tmp')
    $swapBackup=$temp+'.bak'
    $nativePath=Get-NativeFilePath $Path
    $nativeTemp=Get-NativeFilePath $temp
    $nativeSwapBackup=Get-NativeFilePath $swapBackup
    try {
        [IO.File]::WriteAllText($nativeTemp,($Object | ConvertTo-Json -Depth 100),[Text.UTF8Encoding]::new($false))
        if ([IO.File]::Exists($nativePath)) { [IO.File]::Replace($nativeTemp,$nativePath,$nativeSwapBackup) }
        else { [IO.File]::Move($nativeTemp,$nativePath) }
    } finally {
        if ([IO.File]::Exists($nativeTemp)) { [IO.File]::Delete($nativeTemp) }
        if ([IO.File]::Exists($nativeSwapBackup)) { [IO.File]::Delete($nativeSwapBackup) }
    }
}
function Get-Profile([string]$Profile) {
    $guard=[PSCustomObject]@{ enabled=$true; smoothPosition=$false; smoothOrientation=$false; maxLinearSpeed=8.0; maxAngularSpeed=35.0; positionSlack=0.035; angularSlack=0.13962634015954636; maxReportedLinearSpeed=12.0; maxReportedAngularSpeed=50.0; maxPoseTimeOffset=0.25; resetGapSeconds=0.1; reacquireSamples=3; positionMinCutoff=20.0; positionBeta=80.0; orientationMinCutoff=30.0; orientationBeta=6.0 }
    switch ($Profile) {
        'Guard' {}
        'Jitter' { $guard.smoothPosition=$true }
        'Smooth' { $guard.smoothPosition=$true; $guard.smoothOrientation=$true }
        'Baseline' { $guard.enabled=$false }
        'Pimax' { $guard.enabled=$false }
        default { throw "Unknown profile: $Profile" }
    }
    return $guard
}
function Apply-Profile($Settings,[string]$Profile) {
    Set-JsonProperty $Settings 'dreamAirTracking' (Get-Profile $Profile)
    Set-JsonProperty $Settings 'dreamAirPoseSource' $(if($Profile -eq 'Pimax'){'Pimax'}else{'Lighthouse'})
    return $Settings
}
