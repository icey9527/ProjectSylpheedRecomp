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
if (!$GameDataRoot) {
    $localConfig = "$PSScriptRoot/../../assets/runtime.local.json"
    if (Test-Path -LiteralPath $localConfig) {
        $GameDataRoot = (Get-Content -LiteralPath $localConfig -Raw | ConvertFrom-Json).game_data_root
    }
}
$content = [IO.File]::ReadAllText("$PSScriptRoot/../config/project_sylpheed.example.toml")
if ($GameDataRoot) {
    $resolved = (Resolve-Path -LiteralPath $GameDataRoot).Path.Replace('\', '/')
    if (!(Test-Path -LiteralPath "$resolved/BaseLib.dll")) {
        throw 'Development image BaseLib.dll is missing.'
    }
    # JSON string escaping is compatible with a TOML basic string here.
    $encodedPath = ConvertTo-Json -InputObject $resolved -Compress
    $content = $content.Replace('game_data_root = "game-data"', "game_data_root = $encodedPath")
}
[IO.File]::WriteAllText($destination, $content, (New-Object Text.UTF8Encoding($false)))
Write-Host "[OK] Startup config created: $destination"
