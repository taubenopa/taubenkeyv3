; TaubenKey - Windows-Installer (Inno Setup 6)
; Aufruf:  iscc /DVst3Dir="C:\pfad\zu\TaubenKey.vst3" /DAppVersion=1.0.0 installer\TaubenKey.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef Vst3Dir
  #define Vst3Dir "..\build\TaubenKey_artefacts\Release\VST3\TaubenKey.vst3"
#endif

[Setup]
AppId={{B7E1D9C4-6A52-4D3E-9F10-7A1C0DE5A4B1}
AppName=TaubenKey
AppVersion={#AppVersion}
AppVerName=TaubenKey {#AppVersion}
AppPublisher=taubenopa
DefaultDirName={commoncf64}\VST3
DisableProgramGroupPage=yes
DisableDirPage=no
DirExistsWarning=no
UsePreviousAppDir=no
AppendDefaultDirName=no
OutputDir=..\dist
OutputBaseFilename=TaubenKey-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
WizardImageFile=art\wizard.bmp
WizardSmallImageFile=art\wizard_small.bmp
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallFilesDir={commonappdata}\taubenopa\TaubenKey
UninstallDisplayName=TaubenKey (Taube verjagen)
VersionInfoVersion={#AppVersion}
VersionInfoCompany=taubenopa
VersionInfoDescription=TaubenKey VST3 Installer

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Messages]
WelcomeLabel1=Gurr! Eine Taube hat dir etwas mitgebracht.
WelcomeLabel2=Das ist TaubenKey von taubenopa:%nTonart, Stimmung und Tempo - erschnuppert von einem Schwarm Stadttauben.%n%nSchliesse vorher deine DAW, sonst sitzt die Taube vor verschlossenem Fenster.
SelectDirLabel3=Der Taubenschlag fuer VST3-Plugins. Standard passt fast immer - Tauben finden ihr Zuhause von allein.
ButtonNext=&Weiterflattern >
ButtonBack=< &Zurueckhuepfen
ButtonInstall=&Einnisten
ButtonCancel=&Abflug
FinishedHeadingLabel=Die Taube ist gelandet.
FinishedLabelNoIcons=TaubenKey sitzt jetzt im Taubenschlag. Scanne in deiner DAW nach neuen VST3-Plugins und such nach "TaubenKey".

[Dirs]
Name: "{commonappdata}\taubenopa\TaubenKey"

[Files]
Source: "{#Vst3Dir}\*"; DestDir: "{app}\TaubenKey.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "LIESMICH.txt"; DestDir: "{commonappdata}\taubenopa\TaubenKey"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\TaubenKey.vst3"

[Code]
var
  VerhoerPage: TInputOptionWizardPage;

procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel1.Font.Size := 14;
  WizardForm.FinishedHeadingLabel.Font.Size := 14;

  VerhoerPage := CreateInputOptionPage(wpWelcome,
    'Taubenpruefung',
    'Bevor wir einnisten, eine wichtige Frage.',
    'Bist du eine Taube?',
    True, False);
  VerhoerPage.Add('Ja. Gurr.');
  VerhoerPage.Add('Nein, ich bin ein Mensch mit Taubensympathie.');
  VerhoerPage.Add('Ich bin eine Taube, die behauptet, ein Mensch zu sein.');
  VerhoerPage.SelectedValueIndex := 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = VerhoerPage.ID then
  begin
    case VerhoerPage.SelectedValueIndex of
      0: MsgBox('Gurr gurr. Willkommen im Schwarm.', mbInformation, MB_OK);
      1: MsgBox('Akzeptiert. Aber fuettere trotzdem keine Pommes.', mbInformation, MB_OK);
      2: MsgBox('Wir haben nichts gehoert. Weiter geht''s.', mbInformation, MB_OK);
    end;
  end;
end;

procedure CurInstallProgressChanged(CurProgress, MaxProgress: Integer);
var
  P: Integer;
begin
  if MaxProgress <= 0 then Exit;
  P := (CurProgress * 100) div MaxProgress;
  if P < 15 then
    WizardForm.StatusLabel.Caption := 'Die Taube sammelt Kruemel ...'
  else if P < 45 then
    WizardForm.StatusLabel.Caption := 'Die Taube baut am Nest ...'
  else if P < 75 then
    WizardForm.StatusLabel.Caption := 'Die Taube schimpft auf einen Laternenpfahl ...'
  else if P < 95 then
    WizardForm.StatusLabel.Caption := 'Die Taube putzt ihr Schillergefieder ...'
  else
    WizardForm.StatusLabel.Caption := 'Gurr. Fast fertig.';
end;
