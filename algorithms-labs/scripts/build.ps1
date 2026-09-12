# Build and optionally test the CLRS algorithms lab.
param(
    [switch]$Test,
    [string]$Lab = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$make = $null
foreach ($cand in @(
    "mingw32-make",
    "make"
)) {
    $cmd = Get-Command $cand -ErrorAction SilentlyContinue
    if ($cmd) { $make = $cmd.Source; break }
}
if (-not $make) {
    Write-Error "mingw32-make/make not found. Install MSYS2 mingw-w64-ucrt-x86_64-make or add make to PATH."
}

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if ($gcc) {
    $env:CC = $gcc.Source
}

if ($Clean) {
    & $make clean
    exit $LASTEXITCODE
}

if ($Test) {
    if ($Lab) { & $make "test-$Lab" } else { & $make test }
} else {
    if ($Lab) { & $make $Lab } else { & $make }
}
exit $LASTEXITCODE
