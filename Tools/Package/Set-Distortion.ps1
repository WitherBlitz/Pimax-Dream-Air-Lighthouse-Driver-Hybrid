param([Parameter(Mandatory=$true)][string]$ProfileName,[string]$SandboxRoot)
. (Join-Path $PSScriptRoot 'DriverTools.ps1')
. (Join-Path $PSScriptRoot 'DistortionTools.ps1')
Assert-DistortionName $ProfileName
$paths=Get-DriverPaths $SandboxRoot
$profiles=Get-DistortionProfiles $paths.Config
if(!$profiles.ContainsKey($ProfileName)){throw 'Distortion profile is missing, malformed or incompatible with Dream Air.'}
Close-HeadsetSettings $paths
# The GUI can save custom data as it closes. Validate the final files again.
$profiles=Get-DistortionProfiles $paths.Config
if(!$profiles.ContainsKey($ProfileName)){throw 'Distortion profile changed or became invalid while closing headset settings. No settings were changed.'}
# Read after closing the original GUI so its final save is preserved.
$file=Assert-ChildPath $paths.Config (Join-Path $paths.Config 'settings.json')
$settings=Read-JsonOrEmpty $file
$headset=if($settings.PSObject.Properties['dreamAir']){$settings.dreamAir}else{[PSCustomObject]@{}}
if($null -eq $headset -or $headset -is [Array] -or $headset -is [string] -or $headset -is [ValueType]){throw 'Expected dreamAir settings to be a JSON object.'}
$backupRoot=Join-Path $PSScriptRoot 'DistortionBackups'
[IO.Directory]::CreateDirectory($backupRoot) | Out-Null
if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination (Join-Path $backupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString()+'.json'))}
if(!$profiles[$ProfileName].BuiltIn){Add-BundledDistortion $paths.Config $ProfileName}
Set-JsonProperty $headset 'distortionProfile' $ProfileName
Set-JsonProperty $settings 'dreamAir' $headset
Write-JsonAtomic $file $settings
Write-Host "Distortion profile set to $ProfileName."
Write-Host 'SteamVR can stay running; the driver reloads this setting live. Headset settings was closed to avoid competing writes.'
Write-Host 'Tracking, force flags, IPD, eye rotation, other optics, floor and universe settings were preserved.'
