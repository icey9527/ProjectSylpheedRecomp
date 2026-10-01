param([string]$SdkRoot)
$ErrorActionPreference = 'Stop'
$missing = [Collections.Generic.List[string]]::new()
foreach ($tool in @('rexglue.exe', 'clang++', 'cmake', 'ninja')) {
    $command = Get-Command $tool -ErrorAction SilentlyContinue
    if ($command) {
        Write-Host "[OK] $tool : $($command.Source)"
        $versionOutput = & $command.Source --version
        $versionExitCode = $LASTEXITCODE
        $versionOutput | Select-Object -First 1
        # v0.10.0 rexglue prints its version but returns 1; presence is sufficient here.
        if ($versionExitCode -ne 0 -and $tool -ne 'rexglue.exe') { $missing.Add("$tool --version failed") }
    } else {
        Write-Host "[MISSING] $tool"
        $missing.Add($tool)
    }
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (Test-Path -LiteralPath $vswhere) {
    $cppInstallation = & $vswhere -products '*' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($cppInstallation) { Write-Host "[OK] Visual C++ toolset : $cppInstallation" }
    else { $missing.Add('Visual C++ x64/x86 toolset'); Write-Host '[MISSING] Visual C++ x64/x86 toolset' }
} else {
    $missing.Add('Visual Studio C++ Build Tools')
    Write-Host '[MISSING] Visual Studio Installer / C++ Build Tools'
}
$kitsRoot = "${env:ProgramFiles(x86)}/Windows Kits/10"
$sdkHeader = @(Get-ChildItem "$kitsRoot/Include/*/um/Windows.h" -ErrorAction SilentlyContinue)
$sdkLib = @(Get-ChildItem "$kitsRoot/Lib/*/um/x64/kernel32.lib" -ErrorAction SilentlyContinue)
if ($sdkHeader.Count -and $sdkLib.Count) { Write-Host '[OK] Windows SDK headers and x64 libraries' }
else { $missing.Add('Windows SDK headers/libraries'); Write-Host '[MISSING] Windows SDK headers/libraries' }
if (!$SdkRoot) {
    $command = Get-Command rexglue.exe -ErrorAction SilentlyContinue
    if ($command) { $SdkRoot = Split-Path (Split-Path $command.Source) }
}
if ($SdkRoot -and (Test-Path "$SdkRoot/include/rex/rex_app.h") -and
    (Test-Path "$SdkRoot/lib/cmake/rexglue/rexglueConfig.cmake")) {
    Write-Host "[OK] ReXGlue developer package : $SdkRoot"
} else {
    $missing.Add('Full ReXGlue developer package (include/lib/CMake config)')
    Write-Host '[MISSING] Full ReXGlue developer package (include/lib/CMake config)'
}
Write-Host 'This checks discovery only; CMake configure/build is the final verification.'
if ($missing.Count) { throw "Build prerequisites missing: $($missing -join ', ')" }
