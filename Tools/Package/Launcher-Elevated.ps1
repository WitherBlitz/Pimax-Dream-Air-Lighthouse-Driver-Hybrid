param(
    [Parameter(Mandatory=$true)][ValidateSet('Install-Play','Install-OpenPort','Rollback')][string]$Operation,
    [Parameter(Mandatory=$true)][string]$LogPath,
    [ValidateSet('Guard','Jitter','Smooth','Baseline','Pimax')][string]$Profile='Guard'
)
$ErrorActionPreference='Stop'
$launcherResult=0
$transcribing=$false
try {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $LogPath)) | Out-Null
    Start-Transcript -LiteralPath $LogPath -Force | Out-Null
    $transcribing=$true
    switch ($Operation) {
        'Install-Play' { & (Join-Path $PSScriptRoot 'Install.ps1') -Backend Play -Profile $Profile }
        'Install-OpenPort' { & (Join-Path $PSScriptRoot 'Install.ps1') -Backend OpenPort -Profile $Profile }
        'Rollback' { & (Join-Path $PSScriptRoot 'Rollback.ps1') }
    }
} catch {
    $launcherResult=1
    Write-Host ('Operation failed: '+$_.Exception.Message)
} finally {
    if ($transcribing) { Stop-Transcript | Out-Null }
}
exit $launcherResult
