param(
    [Parameter(Mandatory = $true)][string]$GameDataRoot,
    [string]$DevelopmentImage
)
$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (!$DevelopmentImage) {
    $DevelopmentImage = Join-Path $scriptRoot '..\assets\Xacalite_ScriptTeam.exe'
}
$DevelopmentImage = (Resolve-Path -LiteralPath $DevelopmentImage).Path
$root = (Resolve-Path -LiteralPath $GameDataRoot).Path
$destination = Join-Path $root 'BaseLib.dll'
if (Test-Path -LiteralPath $destination) {
    Write-Host "[OK] Existing development image preserved: $destination"
    return
}
if (!(Test-Path -LiteralPath (Join-Path $root 'dat') -PathType Container) -or
    !(Test-Path -LiteralPath (Join-Path $root 'config.ini') -PathType Leaf)) {
    throw 'Select a resource directory containing config.ini and dat/.'
}
# Runtime still loads initial guest data and XEX metadata from this image.
# BaseLib.dll is the development XEX with the project runtime filename; its
# XEX2 header is unchanged. Do not substitute retail default.xex for this code.
Copy-Item -LiteralPath $DevelopmentImage -Destination $destination -ErrorAction Stop
Write-Host "[OK] Development image added for resource testing: $destination"
Write-Host 'Set game_data_root in the EXE-adjacent TOML to this directory.'
Write-Host 'This tests development code with these resources; it does not recompile retail code.'
