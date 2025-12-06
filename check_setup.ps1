$ErrorActionPreference = "SilentlyContinue"

Write-Host "Checking Mermaid Native Setup..." -ForegroundColor Cyan

$vendorDir = "vendor"
if (-not (Test-Path $vendorDir)) { mkdir $vendorDir | Out-Null }

$missing = $false

# Check WebView2Loader.dll
if (-not (Test-Path "$vendorDir/WebView2Loader.dll")) {
    Write-Host "[MISSING] vendor/WebView2Loader.dll" -ForegroundColor Red
    Write-Host "   -> Action: Copy 'WebView2Loader.dll' (x64) here."
    $missing = $true
} else {
    Write-Host "[OK] WebView2Loader.dll found." -ForegroundColor Green
}

# Check SciLexer.dll
if (-not (Test-Path "$vendorDir/SciLexer.dll")) {
    Write-Host "[WARN] vendor/SciLexer.dll" -ForegroundColor Yellow
    Write-Host "   -> Action: Copy 'SciLexer.dll' here (needed for Editor to work)."
} else {
    Write-Host "[OK] SciLexer.dll found." -ForegroundColor Green
}

# Check Headers
if (-not (Test-Path "include/WebView2.h")) {
    Write-Host "[MISSING] include/WebView2.h" -ForegroundColor Red
    $missing = $true
}
if (-not (Test-Path "include/Scintilla.h")) {
    Write-Host "[MISSING] include/Scintilla.h" -ForegroundColor Red
    $missing = $true
}

if ($missing) {
    Write-Host "`nPlease place the missing files and run this script again." -ForegroundColor Yellow
} else {
    Write-Host "`nAll set! You can now run:" -ForegroundColor Green
    Write-Host "cmake -B build"
    Write-Host "cmake --build build"
    Write-Host "./build/bin/MermaidNative.exe"
}
