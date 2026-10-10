function Assert-DistortionName([string]$Name) {
    if([string]::IsNullOrWhiteSpace($Name) -or $Name -cne $Name.Trim() -or $Name.Length -gt 180 -or $Name -in @('.','..') -or $Name.EndsWith('.') -or $Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or $Name -match '[\x00-\x1f\x7f]') { throw 'Choose a valid distortion profile name.' }
    if($Name.Split('.')[0] -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$'){throw 'Choose a valid distortion profile name.'}
}
function Test-DistortionNumber($Value) {
    return ($Value -is [ValueType] -and $Value -isnot [bool] -and $Value -isnot [char] -and ![double]::IsNaN([double]$Value) -and ![double]::IsInfinity([double]$Value))
}
function Test-DistortionCurve($Value,[int]$Minimum,[bool]$Increasing) {
    if($Value -isnot [Array] -or $Value.Count -lt $Minimum -or $Value.Count % 2){return $false}
    for($i=0;$i -lt $Value.Count;$i++){
        if(!(Test-DistortionNumber $Value[$i])){return $false}
        if($Increasing -and $i % 2 -eq 0 -and ($Value[$i] -lt 0 -or ($i -gt 0 -and $Value[$i] -le $Value[$i-2]))){return $false}
    }
    return $true
}
function Test-DreamAirDistortion($Data) {
    if(!$Data -or !$Data.PSObject.Properties['device'] -or !$Data.PSObject.Properties['type']){return $false}
    $compatible=$false
    if($Data.device -is [string]){$compatible=$Data.device -ceq 'DreamAir'}
    elseif($Data.device -is [Array]){foreach($device in $Data.device){if($device -isnot [string]){return $false};if($device -ceq 'DreamAir'){$compatible=$true}}}
    if(!$compatible -or $Data.type -cne 'RadialBezier' -or !$Data.PSObject.Properties['distortions'] -or !(Test-DistortionCurve $Data.distortions 4 $true)){return $false}
    foreach($name in @('distortionsRed','distortionsBlue')){if($Data.PSObject.Properties[$name] -and !(Test-DistortionCurve $Data.$name 0 $false)){return $false}}
    foreach($name in @('offsetX','offsetY','eyeRotationOffset','smoothAmount')){if($Data.PSObject.Properties[$name] -and !(Test-DistortionNumber $Data.$name)){return $false}}
    if($Data.PSObject.Properties['legacySmoothing'] -and $Data.legacySmoothing -isnot [bool]){return $false}
    return $true
}
function Get-DistortionProfiles([string]$Config) {
    $profiles=New-Object 'System.Collections.Generic.Dictionary[string,object]' ([StringComparer]::Ordinal)
    $info=Join-Path $Config 'info.json'
    if(Test-Path -LiteralPath $info){
        try{
            Assert-ChildPath $Config $info | Out-Null
            $metadata=Read-JsonOrEmpty $info
            if($metadata.PSObject.Properties['builtInDistortionProfiles']){
                foreach($entry in $metadata.builtInDistortionProfiles.PSObject.Properties){
                    try{Assert-DistortionName $entry.Name;if(Test-DreamAirDistortion $entry.Value){$profiles[$entry.Name]=[PSCustomObject]@{Name=$entry.Name;SourcePath=$null;BuiltIn=$true}}}catch{}
                }
            }
        }catch{Write-Warning "Could not read built-in distortion metadata: $_"}
    }
    foreach($directory in @((Join-Path $PSScriptRoot 'Distortion'),(Join-Path $Config 'Distortion'))){
        if(!(Test-Path -LiteralPath $directory -PathType Container)){continue}
        foreach($file in Get-ChildItem -LiteralPath $directory -Filter '*.json' -File){
            $name=[IO.Path]::GetFileNameWithoutExtension($file.Name)
            try{Assert-DistortionName $name}catch{continue}
            if($profiles.ContainsKey($name) -and $profiles[$name].BuiltIn){continue}
            $profiles.Remove($name) | Out-Null
            try{
                Assert-ChildPath $directory $file.FullName | Out-Null
                if(Test-DreamAirDistortion (Read-JsonOrEmpty $file.FullName)){$profiles[$name]=[PSCustomObject]@{Name=$name;SourcePath=$file.FullName;BuiltIn=$false}}
            }catch{}
        }
    }
    return $profiles
}
function Add-BundledDistortion([string]$Config,[string]$Name) {
    Assert-DistortionName $Name
    $source=Join-Path (Join-Path $PSScriptRoot 'Distortion') ($Name+'.json')
    if(!(Test-Path -LiteralPath $source)){return}
    Assert-ChildPath $PSScriptRoot $source | Out-Null
    if(!(Test-DreamAirDistortion (Read-JsonOrEmpty $source))){throw 'Bundled distortion data is invalid.'}
    $directory=Assert-ChildPath $Config (Join-Path $Config 'Distortion')
    $target=Assert-ChildPath $Config (Join-Path $directory ($Name+'.json'))
    if(Test-Path -LiteralPath $target){return}
    [IO.Directory]::CreateDirectory((Get-NativeFilePath $directory)) | Out-Null
    $temporary=Join-Path $directory ('.lh1-distortion-'+[guid]::NewGuid().ToString()+'.tmp')
    $nativeTemporary=Get-NativeFilePath $temporary
    try{[IO.File]::WriteAllBytes($nativeTemporary,[IO.File]::ReadAllBytes((Get-NativeFilePath $source)));[IO.File]::Move($nativeTemporary,(Get-NativeFilePath $target))}
    finally{if([IO.File]::Exists($nativeTemporary)){[IO.File]::Delete($nativeTemporary)}}
}
function Close-HeadsetSettings($Paths) {
    if($Paths.Sandbox){return}
    foreach($process in @(Get-Process -Name custom-headset-gui -ErrorAction SilentlyContinue)){
        if($process.HasExited){continue}
        $closed=$process.CloseMainWindow()
        if(!$closed -or !$process.WaitForExit(3000)){
            Stop-Process -Id $process.Id -ErrorAction Stop
            if(!$process.WaitForExit(3000)){throw 'Could not close headset settings. No distortion settings were changed.'}
        }
    }
    if(Get-Process -Name custom-headset-gui -ErrorAction SilentlyContinue){throw 'Headset settings is still running. No distortion settings were changed.'}
}
