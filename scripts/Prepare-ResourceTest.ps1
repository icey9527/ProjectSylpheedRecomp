param(
    [Parameter(Mandatory = $true)][string]$GameDataRoot,
    [string]$DevelopmentImage = "$PSScriptRoot/../../assets/Xacalite_ScriptTeam.exe"
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $GameDataRoot).Path
$destination = Join-Path $root 'Xacalite_ScriptTeam.exe'
if (Test-Path -LiteralPath $destination) {
    Write-Host "[OK] Existing development image preserved: $destination"
    return
}
if (!(Test-Path -LiteralPath (Join-Path $root 'dat') -PathType Container) -or
    !(Test-Path -LiteralPath (Join-Path $root 'config.ini') -PathType Leaf)) {
    throw 'Select a resource directory containing config.ini and dat/.'
}
# Runtime still loads initial guest data and XEX metadata from this image.
# Do not substitute retail default.xex for the recompiled development code.
Copy-Item -LiteralPath $DevelopmentImage -Destination $destination -ErrorAction Stop
Write-Host "[OK] Development image added for resource testing: $destination"
Write-Host 'Set game_data_root in the EXE-adjacent TOML to this directory.'
Write-Host 'This tests development code with these resources; it does not recompile retail code.'
