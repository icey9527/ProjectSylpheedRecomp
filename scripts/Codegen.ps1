$ErrorActionPreference = 'Stop'
Push-Location "$PSScriptRoot/.."
try {
    & rexglue.exe codegen project_sylpheed_manifest.toml
    if ($LASTEXITCODE -ne 0) { throw "ReXGlue codegen failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
