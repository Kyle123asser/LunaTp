[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'Build'),
    [string]$ReferenceBundle,
    [switch]$CaptureGui
)
$ErrorActionPreference = 'Stop'
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
& ctest --test-dir $buildPath -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'C++ tests failed.' }

$verifyArgs = @((Join-Path $PSScriptRoot 'Tests\verify.py'), '--build', $buildPath)
if ($ReferenceBundle) { $verifyArgs += @('--reference', (Resolve-Path -LiteralPath $ReferenceBundle).Path) }
if ($CaptureGui) { $verifyArgs += '--capture-gui' }
& python @verifyArgs
if ($LASTEXITCODE) { throw 'VST3 verification failed.' }
