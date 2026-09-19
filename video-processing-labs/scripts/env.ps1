# Add MSYS2 UCRT64 + make to PATH (run from PowerShell)
# Usage: . .\scripts\env.ps1
$msys = if ($env:MIMO_MSYS2_ROOT) { $env:MIMO_MSYS2_ROOT } else { 'C:\msys64' }
$env:PATH = "$msys\ucrt64\bin;$msys\usr\bin;" + $env:PATH
Write-Host "MSYS2 UCRT64 ready: gcc=$(Get-Command gcc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
