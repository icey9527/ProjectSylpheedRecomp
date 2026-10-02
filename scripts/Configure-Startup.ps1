param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')][string]$Configuration = 'Debug',
    [string]$GameDataRoot
)
$ErrorActionPreference = 'Stop'
$outputDirectory = "$PSScriptRoot/../out/build/win-amd64-$($Configuration.ToLowerInvariant())"
$destination = "$outputDirectory/project_sylpheed.toml"
if (Test-Path -LiteralPath $destination) {
    Write-Host "[OK] Existing startup config preserved: $destination"
    return
}
if (!(Test-Path -LiteralPath $outputDirectory)) { throw 'Build the host first.' }
if (!(Test-Path -LiteralPath (Join-Path $outputDirectory 'BaseLib.dll'))) {
    throw 'BaseLib.dll is missing beside the host executable; rebuild or run Prepare-ResourceTest.ps1.'
}
if (!$GameDataRoot) {
    $localConfig = "$PSScriptRoot/../../assets/runtime.local.json"
    if (Test-Path -LiteralPath $localConfig) {
        $GameDataRoot = (Get-Content -LiteralPath $localConfig -Raw | ConvertFrom-Json).game_data_root
    }
}
$content = [IO.File]::ReadAllText("$PSScriptRoot/../config/project_sylpheed.example.toml")
if ($GameDataRoot) {
    $resolved = (Resolve-Path -LiteralPath $GameDataRoot).Path.Replace('\', '/')
    if (!(Test-Path -LiteralPath "$resolved/config.ini") -or
        !(Test-Path -LiteralPath "$resolved/dat" -PathType Container)) {
        throw 'Resource directory must contain config.ini and dat/.'
    }
    # JSON string escaping is compatible with a TOML basic string here.
    $encodedPath = ConvertTo-Json -InputObject $resolved -Compress
    $content = $content.Replace('game_data_root = "game-data"', "game_data_root = $encodedPath")
}
[IO.File]::WriteAllText($destination, $content, (New-Object Text.UTF8Encoding($false)))
Write-Host "[OK] Startup config created: $destination"
