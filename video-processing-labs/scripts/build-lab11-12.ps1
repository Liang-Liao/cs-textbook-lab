# Build and test lab11 + lab12 (PowerShell)
# Usage: .\scripts\build-lab11-12.ps1
$ErrorActionPreference = 'Stop'
$msys = if ($env:MIMO_MSYS2_ROOT) { $env:MIMO_MSYS2_ROOT } else { 'C:\msys64' }
$env:PATH = "$msys\ucrt64\bin;$msys\usr\bin;" + $env:PATH
$root = Split-Path -Parent $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }

function Run-Lab($dir) {
    Write-Host "==== $dir ====" -ForegroundColor Cyan
    Push-Location (Join-Path $root $dir)
    try {
        make clean
        make
        if ($LASTEXITCODE -ne 0) { throw "make failed in $dir" }
        make test
        if ($LASTEXITCODE -ne 0) { throw "make test failed in $dir" }
        Write-Host "OK $dir" -ForegroundColor Green
    } finally {
        Pop-Location
    }
}

Run-Lab 'labs/lab11-color-yuv'
Run-Lab 'labs/lab12-image-ops'
Write-Host 'ALL LABS OK' -ForegroundColor Green
