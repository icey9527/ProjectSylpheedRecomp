param(
    [string]$LogPath = "$PSScriptRoot/../../logs/codegen-run.log",
    [string]$ReXGlue = 'rexglue.exe'
)
$ErrorActionPreference = 'Stop'
$LogPath = [IO.Path]::GetFullPath($LogPath)
[IO.Directory]::CreateDirectory((Split-Path $LogPath)) | Out-Null
Push-Location "$PSScriptRoot/.."
try {
    & $ReXGlue --log-file $LogPath codegen project_sylpheed_manifest.toml
    if ($LASTEXITCODE -ne 0) { throw "ReXGlue codegen failed with exit code $LASTEXITCODE" }
    & "$PSScriptRoot/Check-GeneratedCode.ps1"
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python -and (Test-Path 'assets/Xacalite_ScriptTeam.map') -and
        (Test-Path 'assets/Xacalite_ScriptTeam.pdb')) {
        & $python.Source "$PSScriptRoot/symbol_index.py" build
        if ($LASTEXITCODE -ne 0) { throw 'Symbol index refresh failed.' }
    } else { Write-Warning 'Symbol index not refreshed; see docs/symbols.md.' }
} finally { Pop-Location }
