param(
    [Parameter(Mandatory)][string]$LogPath,
    [string]$MapPath = "$PSScriptRoot/../assets/Xacalite_ScriptTeam.map",
    [string]$OutputPath = "$PSScriptRoot/../config/map-functions.toml"
)
$ErrorActionPreference = 'Stop'
# Import analysis/runtime destinations validated as function starts by the MAP.
# MAP does not contain reliable function lengths: leave size unset for discovery.
$targetPattern = '(?:0x(?<Target>[0-9A-Fa-f]{8}) from 0x[0-9A-Fa-f]{8}(?::|\s|$)|invalid or unregistered function at guest address 0x(?<Target>[0-9A-Fa-f]{8})(?:\s|$))'
$targets = [regex]::Matches((Get-Content -LiteralPath $LogPath -Raw), $targetPattern) |
    ForEach-Object { $_.Groups['Target'].Value.ToUpperInvariant() } | Sort-Object -Unique
if (!$targets) { throw 'No unresolved call destinations found.' }
$symbols = @{}
foreach ($line in Get-Content -LiteralPath $MapPath) {
    if ($line -match '^\s+[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s') {
        $address = $Matches[2].ToUpperInvariant()
        $symbolName = $Matches[1]
        if (!$symbols.ContainsKey($address)) { $symbols[$address] = @() }
        if ($symbols[$address] -cnotcontains $symbolName) { $symbols[$address] += $symbolName }
    }
}
$lines = @('# MAP-validated function entry seeds for ReXGlue v0.10.0.', '# No guessed lengths; analyzer discovers the function bodies.', '[functions]')
foreach ($address in $targets) {
    if (!$symbols.ContainsKey($address)) { throw "Destination 0x$address is not a MAP function start." }
    foreach ($symbolName in ($symbols[$address] | Sort-Object)) { $lines += "# $symbolName" }
    $lines += "`"0x$address`" = {}"
}
[IO.File]::WriteAllLines([IO.Path]::GetFullPath($OutputPath), $lines, [Text.UTF8Encoding]::new($false))
Write-Host "Imported $($targets.Count) MAP-validated function starts."
