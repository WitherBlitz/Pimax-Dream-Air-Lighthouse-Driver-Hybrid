param([string]$Package=(Join-Path (Split-Path -Parent $PSScriptRoot) 'Tools\Package'))
. (Join-Path $Package 'DriverTools.ps1')
$passed=0
function Check($ok,[string]$message){if(!$ok){throw $message};$script:passed++}
$settings=[pscustomobject]@{forceTracking=$true;forceTrackingHeadsetOnly=$true;keep='user'}
$settings=Apply-Profile $settings 'Baseline'
Check ($settings.forceTracking -and $settings.forceTrackingHeadsetOnly -and $settings.keep -eq 'user') 'Explicit force flags were reset.'
$settings=Apply-Profile $settings 'Pimax'
Check ($settings.dreamAirPoseSource -eq 'Pimax' -and !$settings.dreamAirTracking.enabled -and $settings.forceTracking) 'Pimax source profile failed.'
$settings=Apply-Profile $settings 'Guard'
Check ($settings.dreamAirPoseSource -eq 'Lighthouse' -and $settings.dreamAirTracking.enabled -and $settings.forceTracking) 'Lighthouse source restoration failed.'
$settings=[pscustomobject]@{forceTracking=$false;forceTrackingHeadsetOnly=$false}
$settings=Apply-Profile $settings 'Pimax'
Check (!$settings.forceTracking -and !$settings.forceTrackingHeadsetOnly) 'False force flags changed.'
$settings=Apply-Profile ([pscustomobject]@{}) 'Pimax'
Check (!$settings.PSObject.Properties['forceTracking'] -and !$settings.PSObject.Properties['forceTrackingHeadsetOnly']) 'Absent force flags invented.'
Write-Host "$passed/5 native profile cases passed"
