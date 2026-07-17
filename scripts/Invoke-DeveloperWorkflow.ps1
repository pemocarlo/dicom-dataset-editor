[CmdletBinding()]
param(
    [ValidateSet("dev-check-ninja", "quality-checks", "all-checks")]
    [string]$Preset = "dev-check-ninja"
)

$ErrorActionPreference = "Stop"

$sourceDirectory = Split-Path -Parent $PSScriptRoot
$conanBuild = Join-Path $sourceDirectory "build\Ninja-Debug\generators\conanbuild.bat"
if (-not (Test-Path -LiteralPath $conanBuild)) {
    throw "Missing $conanBuild. Run the documented windows-msvc-debug-ninja Conan install first."
}

$vsInstallDirectory = $env:VSINSTALLDIR
if (-not $vsInstallDirectory) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $vsInstallDirectory = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
    }
}

if (-not $vsInstallDirectory) {
    throw "Visual Studio with the Desktop development with C++ workload was not found."
}

$vsDevCmd = Join-Path $vsInstallDirectory "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "Missing Visual Studio developer environment script: $vsDevCmd"
}

$command = 'call "{0}" -arch=x64 -host_arch=x64 && call "{1}" && set "PATH=!VCToolsInstallDir!bin\Hostx64\x64;!VSINSTALLDIR!VC\Tools\Llvm\x64\bin;!PATH!" && cmake --workflow --preset {2}' -f `
    $vsDevCmd, $conanBuild, $Preset

& cmd.exe /d /v:on /s /c $command
exit $LASTEXITCODE
