param(
    [string]$SdkRoot = $env:REXGLUE_SDK_ROOT,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')][string]$Configuration = 'Debug',
    [ValidateRange(1, 128)][int]$Parallel = 2
)
$ErrorActionPreference = 'Stop'
if (!$SdkRoot) {
    $bundledSdk = "$PSScriptRoot/../../tools/rexglue-sdk-0.10.0-win-amd64/win-amd64"
    if (Test-Path -LiteralPath $bundledSdk) { $SdkRoot = $bundledSdk }
    else {
        $codegen = Get-Command rexglue.exe -ErrorAction SilentlyContinue
        if ($codegen) { $SdkRoot = Split-Path (Split-Path $codegen.Source) }
    }
}
if (!$SdkRoot) { throw 'Pass -SdkRoot with the complete ReXGlue developer package.' }
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$preset = 'win-amd64-' + $Configuration.ToLowerInvariant()
$logDirectory = [IO.Path]::GetFullPath("$PSScriptRoot/../../logs")
[IO.Directory]::CreateDirectory($logDirectory) | Out-Null
function Invoke-BuildCommand {
    param([string]$Command, [string[]]$Arguments, [string]$LogFile)
    $savedPreference = $ErrorActionPreference
    try {
        # Windows PowerShell wraps native stderr as ErrorRecord; retain its text and exit code.
        $ErrorActionPreference = 'Continue'
        & $Command @Arguments 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath $LogFile -Append
        $commandExitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    if ($commandExitCode -ne 0) { throw "$Command failed with exit code $commandExitCode; see $LogFile" }
}
Start-Transcript -Path "$logDirectory/build-$preset.log" -Append | Out-Null
Push-Location "$PSScriptRoot/.."
try {
    . "$PSScriptRoot/Initialize-WindowsToolchain.ps1"
    & "$PSScriptRoot/Check-BuildEnvironment.ps1" -SdkRoot $SdkRoot
    Invoke-BuildCommand -Command 'cmake' -Arguments @('--preset', $preset, "-DCMAKE_PREFIX_PATH=$SdkRoot") -LogFile "$logDirectory/configure-$preset.log"
    Invoke-BuildCommand -Command 'cmake' -Arguments @('--build', '--preset', $preset, '--parallel', "$Parallel") -LogFile "$logDirectory/compile-$preset.log"
    & "$PSScriptRoot/Check-GeneratedCode.ps1"
    & "$PSScriptRoot/Configure-Startup.ps1" -Configuration $Configuration
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python -and (Test-Path 'assets/Xacalite_ScriptTeam.map') -and
        (Test-Path 'assets/Xacalite_ScriptTeam.pdb')) {
        & $python.Source "$PSScriptRoot/symbol_index.py" build | Out-File -Encoding utf8 "$logDirectory/symbols-$preset.json"
        if ($LASTEXITCODE -ne 0) { throw 'Host built, but symbol index refresh failed.' }
    } else { Write-Warning 'Symbol index not refreshed; see docs/symbols.md.' }
    Write-Host "[OK] Host built in out/build/$preset. Game behavior has not been verified."
} finally {
    Pop-Location
    Stop-Transcript | Out-Null
}
