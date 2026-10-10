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
function Write-JsonAtomic([string]$Path,$Object) {
    $parent=Split-Path -Parent $Path
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    $temp=Join-Path $parent ('.lh1-json-'+[guid]::NewGuid().ToString()+'.tmp')
    $swapBackup=$temp+'.bak'
    try {
        [IO.File]::WriteAllText($temp,($Object | ConvertTo-Json -Depth 100),[Text.UTF8Encoding]::new($false))
        if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temp,$Path,$swapBackup) }
        else { [IO.File]::Move($temp,$Path) }
    } finally {
        if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
        if (Test-Path -LiteralPath $swapBackup) { Remove-Item -LiteralPath $swapBackup -Force }
    }
}
function Get-Profile([string]$Profile) {
    $guard=[PSCustomObject]@{ enabled=$true; smoothPosition=$false; smoothOrientation=$false; maxLinearSpeed=8.0; maxAngularSpeed=35.0; positionSlack=0.035; angularSlack=0.13962634015954636; maxReportedLinearSpeed=12.0; maxReportedAngularSpeed=50.0; maxPoseTimeOffset=0.25; resetGapSeconds=0.1; reacquireSamples=3; positionMinCutoff=20.0; positionBeta=80.0; orientationMinCutoff=30.0; orientationBeta=6.0 }
    switch ($Profile) {
        'Guard' {}
        'Jitter' { $guard.smoothPosition=$true }
        'Smooth' { $guard.smoothPosition=$true; $guard.smoothOrientation=$true }
        'Baseline' { $guard.enabled=$false }
        default { throw "Unknown profile: $Profile" }
    }
    return $guard
}
function Apply-Profile($Settings,[string]$Profile) {
    Set-JsonProperty $Settings 'dreamAirTracking' (Get-Profile $Profile)
    Set-JsonProperty $Settings 'forceTracking' $false
    Set-JsonProperty $Settings 'forceTrackingHeadsetOnly' $false
    return $Settings
}
