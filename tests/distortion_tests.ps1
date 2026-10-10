param([Parameter(Mandatory=$true)][string]$Package,[Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
. (Join-Path $Package 'DriverTools.ps1')
. (Join-Path $Package 'DistortionTools.ps1')
$testBase=[IO.Path]::GetFullPath($TestRoot)
[IO.Directory]::CreateDirectory($testBase) | Out-Null
$passed=0;$failed=0
function Check($ok,$message) { if (!$ok) { throw $message } }
function Test([string]$name,[scriptblock]$run) {
    try { & $run; $script:passed++; Write-Host "PASS $name" }
    catch { $script:failed++; Write-Host "FAIL ${name}: $_" }
}
function New-Sandbox {
    $root=Join-Path $testBase ([guid]::NewGuid().ToString())
    foreach($path in @('SteamVR\drivers','SteamConfig','CustomHeadset\Distortion')){[IO.Directory]::CreateDirectory((Join-Path $root $path)) | Out-Null}
    [IO.File]::WriteAllText((Join-Path $root '.dreamair-lh1-sandbox'),'test')
    [IO.File]::WriteAllText((Join-Path $root 'CustomHeadset\info.json'),'{"builtInDistortionProfiles":{"Dream Air Default":{"device":["DreamAir","Other"],"type":"RadialBezier","distortions":[0,0,55,117]},"Dream Air like Pimax":{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,115]},"Wrong headset":{"device":"DreamAirSE","type":"RadialBezier","distortions":[0,0,55,115]}}}')
    return $root
}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Equal-Json($Left,$Right){
    if($null -eq $Left -or $null -eq $Right){return $null -eq $Left -and $null -eq $Right}
    if($Left -is [Array]){
        if($Right -isnot [Array] -or $Left.Count -ne $Right.Count){return $false}
        for($index=0;$index -lt $Left.Count;$index++){if(!(Equal-Json $Left[$index] $Right[$index])){return $false}};return $true
    }
    if($Left -is [string] -or $Left -is [ValueType]){return $Left -ceq $Right}
    if($Right -is [Array] -or $Right -is [string] -or $Right -is [ValueType]){return $false}
    if(@($Left.PSObject.Properties).Count -ne @($Right.PSObject.Properties).Count){return $false}
    foreach($property in $Left.PSObject.Properties){if(!$Right.PSObject.Properties[$property.Name] -or !(Equal-Json $property.Value $Right.($property.Name))){return $false}}
    return $true
}
function Apply([string]$root,[string]$name){& (Join-Path $Package 'Set-Distortion.ps1') -SandboxRoot $root -ProfileName $name}
$default='Dream Air Maximum Comfort 100pct Overlap'
Test 'Applying a builtin changes only distortion and retains exact backup bytes' {
    $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json'
    $before='{"dreamAir":{"distortionProfile":"old","ipd":64.1,"eyeRotation":0,"useNativePimaxTracking":true,"fovZoom":1.05},"dreamAirPoseSource":"Pimax","dreamAirTracking":{"enabled":false},"forceTracking":true,"forceTrackingHeadsetOnly":true,"room":{"floor":1.905,"universe":31},"unknown":{"x":[1,2,3]}}'
    [IO.File]::WriteAllText($file,$before);$hash=Hash $file
    Apply $root 'Dream Air Default'
    $actual=Read-JsonOrEmpty $file;$expected=$before | ConvertFrom-Json
    $expected.dreamAir.distortionProfile='Dream Air Default'
    Check (Equal-Json $actual $expected) 'Unrelated settings changed.'
    Check (@(Get-ChildItem -LiteralPath (Join-Path $Package 'DistortionBackups') -Filter *.json | Where-Object {(Hash $_.FullName) -eq $hash}).Count -gt 0) 'Exact settings backup missing.'
}
Test 'Bundled custom applies exact data without touching other custom files' {
    $root=New-Sandbox;$custom=Join-Path $root 'CustomHeadset\Distortion\personal.json'
    [IO.File]::WriteAllText($custom,'{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,116]}');$hash=Hash $custom
    Apply $root $default
    Check ((Read-JsonOrEmpty (Join-Path $root 'CustomHeadset\settings.json')).dreamAir.distortionProfile -ceq $default) 'Custom profile not selected.'
    Check ((Hash (Join-Path $root "CustomHeadset\Distortion\$default.json")) -ceq '00B17F0D11166C0796F344EB3F414FE113E898CBDA04E9C40A94C05B0FC8806E') 'Bundled custom bytes differ.'
    Check ((Hash $custom) -eq $hash) 'Another custom file changed.'
}
Test 'Compatible custom device arrays are accepted and existing data never overwritten' {
    $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\Distortion\My optics.json'
    [IO.File]::WriteAllText($file,'{"device":["Other","DreamAir"],"type":"RadialBezier","distortions":[0,0,55,117]}');$hash=Hash $file
    Apply $root 'My optics'
    Check ((Read-JsonOrEmpty (Join-Path $root 'CustomHeadset\settings.json')).dreamAir.distortionProfile -ceq 'My optics') 'Compatible custom file not selected.'
    Check ((Hash $file) -eq $hash) 'Existing custom data changed.'
}
Test 'Unknown traversal incompatible malformed and invalid curves reject before writes' {
    foreach($item in @(
        @('missing',$null),@('..\outside',$null),@('C:\outside',$null),@('Wrong headset',$null),
        @('broken','{bad'),@('wrong','{"device":"DreamAirSE","type":"RadialBezier","distortions":[0,0,55,117]}'),
        @('odd','{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55]}'),
        @('text','{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,"bad"]}'),
        @('kind','{"device":"DreamAir","type":"Unknown","distortions":[0,0,55,117]}'))){
        $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json';[IO.File]::WriteAllText($file,'{"dreamAir":{"distortionProfile":"keep"},"forceTracking":true}');$hash=Hash $file
        if($item[1]){[IO.File]::WriteAllText((Join-Path $root "CustomHeadset\Distortion\$($item[0]).json"),$item[1])}
        $rejected=$false;try{Apply $root $item[0]}catch{$rejected=$true}
        Check ($rejected -and (Hash $file) -eq $hash) "Unsafe profile accepted or settings changed: $($item[0])"
    }
}
Test 'A malformed existing default cannot be silently replaced by the bundled profile' {
    $root=New-Sandbox;$file=Join-Path $root "CustomHeadset\Distortion\$default.json";[IO.File]::WriteAllText($file,'{ broken');$hash=Hash $file
    $rejected=$false;try{Apply $root $default}catch{$rejected=$true}
    Check ($rejected -and (Hash $file) -eq $hash -and !(Test-Path -LiteralPath (Join-Path $root 'CustomHeadset\settings.json'))) 'Bad existing file was replaced or selected.'
}
foreach($fixture in @(
    @('red singleton','"distortions":[0,0,55,117],"distortionsRed":[0,0.3]'),
    @('blue duplicate degree','"distortions":[0,0,55,117],"distortionsBlue":[0,-0.3,0,-0.4]'),
    @('color float-collapsed degree','"distortions":[0,0,55,117],"distortionsRed":[20.00000001,0.3,20.00000002,0.4]'),
    @('main float-collapsed degree','"distortions":[0,0,20.00000001,50,20.00000002,51]'),
    @('main float overflow','"distortions":[0,0,55,1e100]'),
    @('color float overflow','"distortions":[0,0,55,117],"distortionsRed":[0,0.3,55,1e100]'),
    @('offset float overflow','"distortions":[0,0,55,117],"offsetX":1e100'),
    @('optional schema case','"distortions":[0,0,55,117],"DistortionsRed":[0,0.3,55,0.4]'))){
    Test "Reject $($fixture[0]) before settings profile or backup writes" {
        $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json';$profile=Join-Path $root 'CustomHeadset\Distortion\Unsafe.json'
        [IO.File]::WriteAllText($file,'{"dreamAir":{"distortionProfile":"keep"},"forceTracking":true}')
        [IO.File]::WriteAllText($profile,('{"device":"DreamAir","type":"RadialBezier",'+$fixture[1]+'}'))
        $hash=Hash $file;$profileHash=Hash $profile;$backups=@(Get-ChildItem -LiteralPath (Join-Path $Package 'DistortionBackups') -Filter *.json).Count
        $rejected=$false;try{Apply $root 'Unsafe'}catch{$rejected=$true}
        Check ($rejected -and (Hash $file) -eq $hash -and (Hash $profile) -eq $profileHash -and @(Get-ChildItem -LiteralPath (Join-Path $Package 'DistortionBackups') -Filter *.json).Count -eq $backups) 'Unsafe profile accepted or files/backups changed.'
    }
}
Test 'Required schema field casing is exact and cannot silently fall back in the native parser' {
    $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json';[IO.File]::WriteAllText($file,'{"dreamAir":{"distortionProfile":"keep"}}');$hash=Hash $file
    [IO.File]::WriteAllText((Join-Path $root 'CustomHeadset\Distortion\Case.json'),'{"Device":"DreamAir","Type":"RadialBezier","Distortions":[0,0,55,117]}')
    $rejected=$false;try{Apply $root 'Case'}catch{$rejected=$true}
    Check ($rejected -and (Hash $file) -eq $hash) 'Incorrectly cased schema selected or settings changed.'
}
Test 'Commented profiles retain quoted markers escaped quotes negative offsets and exact profile bytes' {
    $root=New-Sandbox;$profile=Join-Path $root 'CustomHeadset\Distortion\Commented.json'
    $json=@'
{ // line comment
"device":"DreamAir", /* block comment */ "type":"RadialBezier","distortions":[0,0,55,117],
"distortionsRed":[],"distortionsBlue":[0,-0.3,55,-0.3],
"description":"https://example.test/ // literal /* literal */ \"quoted\" and \\ path"}
'@
    [IO.File]::WriteAllText($profile,$json);$hash=Hash $profile
    Check ((Read-DistortionJson $profile).description -ceq 'https://example.test/ // literal /* literal */ "quoted" and \ path') 'Quoted markers or escape sequences changed while reading.'
    Apply $root 'Commented'
    Check ((Read-JsonOrEmpty (Join-Path $root 'CustomHeadset\settings.json')).dreamAir.distortionProfile -ceq 'Commented' -and (Hash $profile) -eq $hash) 'Valid commented profile rejected or rewritten.'
}
Test 'Comment removal preserves token separation and rejects unterminated comments without writes' {
    foreach($json in @('{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,1/* comment */17]}','{"device":"DreamAir","type":"RadialBezier","distortions":[0,0,55,117]/* unterminated')){
        $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json';[IO.File]::WriteAllText($file,'{"dreamAir":{"distortionProfile":"keep"}}');$hash=Hash $file
        [IO.File]::WriteAllText((Join-Path $root 'CustomHeadset\Distortion\Commented.json'),$json)
        $rejected=$false;try{Apply $root 'Commented'}catch{$rejected=$true}
        Check ($rejected -and (Hash $file) -eq $hash) 'Invalid comment-bearing tokens selected or settings changed.'
    }
}
Test 'Malformed settings reject before adding bundled data' {
    $root=New-Sandbox;$file=Join-Path $root 'CustomHeadset\settings.json';[IO.File]::WriteAllText($file,'{bad');$hash=Hash $file
    $rejected=$false;try{Apply $root $default}catch{$rejected=$true}
    Check ($rejected -and (Hash $file) -eq $hash -and !(Test-Path -LiteralPath (Join-Path $root "CustomHeadset\Distortion\$default.json"))) 'Malformed settings mutated files.'
}
Test 'Bundled data copies byte-exactly when its target exceeds MAX_PATH' {
    $root=New-Sandbox;$config=Join-Path $root ('long-config-'+('x'*(245-$root.Length-13)))
    [IO.Directory]::CreateDirectory((Get-NativeFilePath $config)) | Out-Null
    $target=Join-Path $config "Distortion\$default.json"
    Check ($target.Length -gt 260) 'Long-path fixture was too short.'
    Add-BundledDistortion $config $default
    $readback=Join-Path $root 'readback.json';[IO.File]::WriteAllBytes($readback,[IO.File]::ReadAllBytes((Get-NativeFilePath $target)))
    Check ((Hash $readback) -ceq '00B17F0D11166C0796F344EB3F414FE113E898CBDA04E9C40A94C05B0FC8806E') 'Long-path profile bytes differ.'
}
Test 'Fresh installation selects bundled custom while an explicit builtin selection survives installation' {
    $root=New-Sandbox
    & (Join-Path $Package 'Install.ps1') -SandboxRoot $root -Backend OpenPort -Profile Pimax
    $file=Join-Path $root 'CustomHeadset\settings.json'
    Check ((Read-JsonOrEmpty $file).dreamAir.distortionProfile -ceq $default) 'Fresh install did not select custom default.'
    Apply $root 'Dream Air like Pimax'
    & (Join-Path $Package 'Install.ps1') -SandboxRoot $root -Backend OpenPort -Profile Pimax
    Check ((Read-JsonOrEmpty $file).dreamAir.distortionProfile -ceq 'Dream Air like Pimax') 'Reinstallation reset explicit distortion choice.'
}
Write-Host "$passed/$($passed+$failed) distortion cases passed"
if($failed){exit 1}
