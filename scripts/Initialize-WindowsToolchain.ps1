# Dot-source this script to prepare the current PowerShell process only.
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer not found.' }
$vsPath = & $vswhere -products '*' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Visual C++ x64/x86 build tools not found.' }
Import-Module "$vsPath/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$toolDirectories = @(
    "$vsPath/VC/Tools/Llvm/x64/bin",
    "$vsPath/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin",
    "$vsPath/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
) | Where-Object { Test-Path -LiteralPath $_ }
$env:Path = ($toolDirectories -join ';') + ';' + $env:Path
$compiler = Get-Command clang++ -ErrorAction Stop
$version = & $compiler.Source --version
if ($LASTEXITCODE -ne 0 -or !($version -match 'Target: x86_64-pc-windows-msvc')) {
    throw 'Expected a Windows x64/MSVC Clang toolchain.'
}
Write-Host '[OK] Windows x64 toolchain loaded for this process.'
