$ErrorActionPreference = 'Stop'
$directory = "$PSScriptRoot/../generated/xacalite_scriptteam"
$files = @(Get-ChildItem -LiteralPath $directory -Filter '*.cpp' -ErrorAction Stop)
if (!$files.Count) { throw 'No generated C++ files found; run codegen first.' }
$fatal = @($files | Select-String -Pattern 'FATAL: unresolved|REX_FATAL\("Unresolved')
if ($fatal.Count) { throw "Generated unresolved-call fatal stubs remain: $($fatal.Count)" }
Write-Host '[OK] No generated unresolved-call fatal stubs. Instruction semantics still require separate verification.'
