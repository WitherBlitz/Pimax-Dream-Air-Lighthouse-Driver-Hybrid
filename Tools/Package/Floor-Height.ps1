[CmdletBinding(DefaultParameterSetName='Custom')]
param(
    [Parameter(Mandatory=$true,ParameterSetName='Preset')][switch]$Preset6ft3,
    [Parameter(Mandatory=$true,ParameterSetName='Feet')][ValidateRange(0,32)][int]$Feet,
    [Parameter(ParameterSetName='Feet')][ValidateRange(0,11.999999)][double]$Inches=0,
    [Parameter(Mandatory=$true,ParameterSetName='Centimeters')][ValidateRange(0,1000)][double]$Centimeters,
    [Parameter(Mandatory=$true,ParameterSetName='Meters')][ValidateRange(0,10)][double]$Meters,
    [Parameter(Mandatory=$true,ParameterSetName='Read')][switch]$Read,
    [Parameter(Mandatory=$true,ParameterSetName='Restore')][switch]$Restore,
    [Parameter(ParameterSetName='Restore')][ValidatePattern('^[0-9-]{1,99}$')][string]$BackupId,
    [ValidateRange(0,60)][int]$Delay=5,
    [string]$BackupDirectory
)
Set-StrictMode -Version 2
$ErrorActionPreference='Stop'
$invariant=[Globalization.CultureInfo]::InvariantCulture
function Read-HeightNumber([string]$Prompt,[double]$Default) {
    $answer=Read-Host "$Prompt [$($Default.ToString('R',$invariant))]"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $Default }
    $number=0.0
    if (![double]::TryParse($answer,[Globalization.NumberStyles]::Float,$invariant,[ref]$number) -or [double]::IsNaN($number) -or [double]::IsInfinity($number)) {
        throw 'Enter a finite number, using a decimal point for fractions.'
    }
    return $number
}
$tool=Join-Path $PSScriptRoot 'Tools\FloorHeight.exe'
if (!(Test-Path -LiteralPath $tool)) { throw 'FloorHeight.exe is missing. Extract the complete package.' }
if (!$BackupDirectory) { $BackupDirectory=Join-Path $PSScriptRoot 'FloorBackups' }
$floorArguments=@('--state-dir',$BackupDirectory)
switch ($PSCmdlet.ParameterSetName) {
    'Preset' { $floorArguments+=@('--feet','6','--inches','3') }
    'Feet' { $floorArguments+=@('--feet',$Feet.ToString($invariant),'--inches',$Inches.ToString('R',$invariant)) }
    'Centimeters' { $floorArguments+=@('--centimeters',$Centimeters.ToString('R',$invariant)) }
    'Meters' { $floorArguments+=@('--meters',$Meters.ToString('R',$invariant)) }
    'Read' { $floorArguments+='--show' }
    'Restore' { $floorArguments+='--restore';if ($BackupId) { $floorArguments+=@('--backup-id',$BackupId) } }
    default {
        Write-Host 'Set a fixed floor below the headset. Keep SteamVR running and hold the headset at your reference position.'
        Write-Host 'Units: 1 = feet/inches, 2 = centimetres, 3 = metres. Press Enter for feet/inches.'
        $unit=Read-Host 'Units'
        switch ($unit) {
            { $_ -eq '' -or $_ -eq '1' } {
                $customFeet=Read-HeightNumber 'Whole feet' 6
                $customInches=Read-HeightNumber 'Inches (0 to less than 12)' 3
                $floorArguments+=@('--feet',$customFeet.ToString('R',$invariant),'--inches',$customInches.ToString('R',$invariant))
            }
            '2' { $height=Read-HeightNumber 'Centimetres' 190.5;$floorArguments+=@('--centimeters',$height.ToString('R',$invariant)) }
            '3' { $height=Read-HeightNumber 'Metres' 1.905;$floorArguments+=@('--meters',$height.ToString('R',$invariant)) }
            default { throw 'Choose 1, 2, or 3.' }
        }
    }
}
if ($PSCmdlet.ParameterSetName -ne 'Read' -and $PSCmdlet.ParameterSetName -ne 'Restore') { $floorArguments+=@('--delay',$Delay.ToString($invariant)) }
& $tool @floorArguments
exit $LASTEXITCODE
