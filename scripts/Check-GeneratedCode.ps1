param([string]$Directory = "$PSScriptRoot/../generated/xacalite_scriptteam")
$ErrorActionPreference = 'Stop'
$files = @(Get-ChildItem -LiteralPath $Directory -Filter '*.cpp' -ErrorAction Stop)
if (!$files.Count) { throw 'No generated C++ files found; run codegen first.' }
$fatal = @($files | Select-String -Pattern 'FATAL: unresolved|REX_FATAL\("Unresolved')
if ($fatal.Count) { throw "Generated unresolved-call fatal stubs remain: $($fatal.Count)" }
$missingLabels = [Collections.Generic.List[string]]::new()
foreach ($file in $files) {
    $parts = [regex]::Split([IO.File]::ReadAllText($file.FullName), 'DEFINE_REX_FUNC\((\w+)\)')
    for ($index = 1; $index -lt $parts.Count; $index += 2) {
        $functionName = $parts[$index]
        $body = $parts[$index + 1]
        $labels = @{}
        foreach ($match in [regex]::Matches($body, '(?m)^(loc_[0-9A-F]+):')) { $labels[$match.Groups[1].Value] = $true }
        foreach ($match in [regex]::Matches($body, 'goto (loc_[0-9A-F]+)')) {
            $target = $match.Groups[1].Value
            if (!$labels.ContainsKey($target)) { $missingLabels.Add("$functionName -> $target ($($file.Name))") }
        }
    }
}
if ($missingLabels.Count) { throw "Generated goto targets missing in their function: $($missingLabels -join '; ')" }
Write-Host '[OK] No unresolved-call fatal stubs or missing local goto labels. Instruction semantics still require separate verification.'
