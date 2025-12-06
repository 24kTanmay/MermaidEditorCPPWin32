$ErrorActionPreference = "Stop"

$scintillaUrl = "https://www.scintilla.org/scintilla552.zip"
$sciteUrl = "https://www.scintilla.org/wscite552.zip"

Write-Host "1. Downloading Scintilla Source (headers)..."
Invoke-WebRequest -Uri $scintillaUrl -OutFile "scintilla.zip"

Write-Host "2. Downloading SciTE (for DLL)..."
Invoke-WebRequest -Uri $sciteUrl -OutFile "scite.zip"

Write-Host "3. Extracting Headers..."
Expand-Archive -Path "scintilla.zip" -DestinationPath "temp_scintilla" -Force
$headerSrc = Get-ChildItem -Path "temp_scintilla" -Recurse -Filter "Scintilla.h" | Select-Object -First 1
$lexerSrc = Get-ChildItem -Path "temp_scintilla" -Recurse -Filter "SciLexer.h" | Select-Object -First 1

Copy-Item $headerSrc.FullName -Destination "include/"
Copy-Item $lexerSrc.FullName -Destination "include/"

Write-Host "4. Extracting DLL..."
Expand-Archive -Path "scite.zip" -DestinationPath "temp_scite" -Force
# Ensure bin exists
New-Item -ItemType Directory -Force -Path "build/bin" | Out-Null
Copy-Item "temp_scite/wscite/SciLexer.dll" -Destination "build/bin/"
Copy-Item "temp_scite/wscite/SciLexer.dll" -Destination "build/" # For linker copy if needed

Write-Host "5. Cleanup..."
Remove-Item "scintilla.zip"
Remove-Item "scite.zip"
Remove-Item "temp_scintilla" -Recurse -Force
Remove-Item "temp_scite" -Recurse -Force

Write-Host "Done! Scintilla files installed."
