[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'Build'),
    [string]$JuceArchive,
    [string]$VcVarsPath,
    [switch]$SkipTests,
    [ValidateRange(1,64)][int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'

# Use the current x64 developer environment or discover an existing MSVC install.
if ($VcVarsPath -or -not $env:VCToolsInstallDir -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    if ($VcVarsPath) {
        $vcvars = (Resolve-Path -LiteralPath $VcVarsPath).Path
    } else {
        $vswhere = Join-Path "${env:ProgramFiles(x86)}" 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            throw 'Run this script in an x64 Visual Studio Developer PowerShell, or provide an existing MSVC installation discoverable by vswhere.'
        }
        $installations = & $vswhere -all -products '*' -property installationPath
        $vcvars = $installations | ForEach-Object {
            Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat'
        } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        if (-not $vcvars) { throw 'No Visual Studio installation with vcvars64.bat was found.' }
    }
    $variables = & $env:ComSpec /d /s /c ('call "' + $vcvars + '" >nul && set')
    if ($LASTEXITCODE) { throw 'Could not initialize MSVC x64 for this process.' }
    foreach ($line in $variables) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}
Get-Command cmake,ninja -ErrorAction Stop | Out-Null
$buildPath = [IO.Path]::GetFullPath($BuildDirectory)
$argsForCmake = @('-S', $PSScriptRoot, '-B', $buildPath, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
$argsForCmake += "-DLUNA_BUILD_TESTS=$(-not $SkipTests)"
if ($JuceArchive) {
    $archive = (Resolve-Path -LiteralPath $JuceArchive).Path
    $argsForCmake += "-DLUNA_JUCE_ARCHIVE=$archive"
} else {
    $argsForCmake += '-DLUNA_JUCE_ARCHIVE='
}
# Also covers the nested JUCE helper on MSVC installations with a PDB tool mismatch.
$previousCompilerFlags = $env:_CL_
try {
    $env:_CL_ = "$previousCompilerFlags /Z7 /utf-8".Trim()
    & cmake @argsForCmake
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & cmake --build $buildPath --config Release --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Build failed.' }
} finally {
    $env:_CL_ = $previousCompilerFlags
}
Write-Host "VST3 bundle: $(Join-Path $buildPath 'LunaTp_artefacts\Release\VST3\Luna Tp.vst3')"
