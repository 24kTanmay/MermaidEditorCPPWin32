[Setup]
AppName=MermaidEditor
AppVersion=1.0
DefaultDirName={pf}\MermaidEditor
DefaultGroupName=MermaidEditor
OutputDir=.\installer
OutputBaseFilename=MermaidEditorSetup
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "C:\path\to\your\project\bin\Release\net8.0-windows\win-x64\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\MermaidEditor"; Filename: "{app}\MermaidEditor.exe"
Name: "{commondesktop}\MermaidEditor"; Filename: "{app}\MermaidEditor.exe"; Tasks: desktopicon

[Tasks]
Name: desktopicon; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\MermaidEditor.exe"; Description: "Launch MermaidEditor"; Flags: nowait postinstall skipifsilent