; SPDX-License-Identifier: GPL-2.0-or-later
; Copyright (C) 2026 Oliver Cassel
#ifndef RepoRoot
  #define RepoRoot ".."
#endif
#define PluginName "obs-sacn-crossfade"

[Setup]
AppId=MA_OBS_MediaLink_OBS_x64
AppName=MA OBS MediaLink
AppVersion=0.46.0
AppVerName=MA OBS MediaLink v0.46 BETA
AppPublisher=Oliver Cassel
AppPublisherURL=https://github.com/selterwasser-ml/MA-OBS-MediaLink
AppSupportURL=https://github.com/selterwasser-ml/MA-OBS-MediaLink/issues
VersionInfoVersion=0.46.0.1
VersionInfoDescription=MA OBS MediaLink Setup
SetupIconFile={#RepoRoot}\OBS\data\medialink.ico
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginName}
UsePreviousAppDir=no
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
LicenseFile={#RepoRoot}\LICENSE
OutputDir={#RepoRoot}\dist
OutputBaseFilename=MA_OBS_MediaLink_v046_Setup_x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupMutex=MA_OBS_MediaLink_Setup
CloseApplications=no
RestartApplications=no
RestartIfNeededByRun=no
UninstallDisplayName=MA OBS MediaLink v0.46 BETA
UninstallDisplayIcon={app}\data\medialink.ico
UninstallFilesDir={app}\uninstall
UninstallLogMode=append

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
german.OBSPage=OBS-Installation
german.OBSDescription=Bitte den Ordner deiner normalen OBS-Studio-Installation auswählen.
german.OBSHelp=Unterstützt wird OBS Studio 33 für Windows x64. Wähle den Hauptordner, zum Beispiel C:\Program Files\obs-studio. Portable OBS-Installationen werden von diesem Setup nicht unterstützt.
german.OBSFolder=OBS-Ordner:
german.InvalidOBS=Im ausgewählten Ordner wurde kein OBS Studio 33 für Windows x64 gefunden.
german.PortableOBS=Diese OBS-Installation ist als portabel markiert. Bitte eine normale OBS-Installation auswählen.
german.RunningOBS=OBS ist noch geöffnet. Bitte OBS vollständig schließen und den Vorgang erneut starten.
german.ProcessError=Die Prüfung auf laufendes OBS ist fehlgeschlagen. Windows neu starten und den Vorgang erneut versuchen.
german.DuplicateOBS=Es gibt bereits eine MediaLink-DLL in einem anderen OBS-Pluginordner. Bitte diese Kopie zuerst sichern und entfernen: %1
german.Finished=MediaLink wurde installiert. Starte OBS und öffne Werkzeuge > MA OBS MediaLink. Im Tab Allgemein findest du MA-Paket und Anleitungen öffnen.
german.RemoveTitle=MA OBS MediaLink deinstallieren
german.RemoveIntro=Die Programmdateien und das mitgelieferte MA-Paket werden entfernt. Deine OBS-Szenen, Profile und anderen Plugins bleiben erhalten.
german.DeleteSettings=MediaLink-Einstellungen ebenfalls löschen
german.SettingsAccount=Betroffenes Windows-Konto: %1%nEinstellungsordner: %2
german.KeepSettings=Ohne Häkchen bleiben deine MediaLink-Regeln und Einstellungen für eine spätere Neuinstallation erhalten.
german.ContinueRemove=Weiter
german.CancelRemove=Abbrechen
german.SettingsError=Die MediaLink-Einstellungen konnten nicht vollständig entfernt werden. Bitte diesen Ordner nach dem Schließen von OBS prüfen:%n%1
english.OBSPage=OBS installation
english.OBSDescription=Select your standard OBS Studio installation folder.
english.OBSHelp=Requires OBS Studio 33 for Windows x64. Select the main folder, for example C:\Program Files\obs-studio. Portable OBS installations are not supported by this setup.
english.OBSFolder=OBS folder:
english.InvalidOBS=The selected folder does not contain OBS Studio 33 for Windows x64.
english.PortableOBS=This OBS installation is marked as portable. Please select a standard OBS installation.
english.RunningOBS=OBS is still running. Close OBS completely and start this operation again.
english.ProcessError=Could not check for running OBS. Restart Windows and try again.
english.DuplicateOBS=A MediaLink DLL already exists in another OBS plugin folder. Back up and remove that copy first: %1
english.Finished=MediaLink has been installed. Start OBS and open Tools > MA OBS MediaLink. The General tab opens the installed MA package and guides.
english.RemoveTitle=Uninstall MA OBS MediaLink
english.RemoveIntro=The program files and bundled MA package will be removed. Your OBS scenes, profiles and other plugins will be kept.
english.DeleteSettings=Also delete MediaLink settings
english.SettingsAccount=Windows account: %1%nSettings folder: %2
english.KeepSettings=Leave this unchecked to keep your MediaLink rules and settings for a later installation.
english.ContinueRemove=Continue
english.CancelRemove=Cancel
english.SettingsError=Could not remove all MediaLink settings. Close OBS and check this folder:%n%1

[Messages]
german.FinishedLabel={cm:Finished}
english.FinishedLabel={cm:Finished}
german.ConfirmUninstall=Soll %1 deinstalliert werden? Die Auswahl zu den gespeicherten Einstellungen wird dabei berücksichtigt.
english.ConfirmUninstall=Uninstall %1? Your selection for saved settings will also be applied.

[Files]
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\bin\64bit\{#PluginName}.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\crossfade.effect"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\medialink.ico"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\START_HIER.html"; DestDir: "{app}\data\ma-paket"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\grandMA3\MA3\MA_OBS_MediaLink.xml"; DestDir: "{app}\data\ma-paket\grandMA3\MA3"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\grandMA3\MA3\MA_OBS_MediaLink.lua"; DestDir: "{app}\data\ma-paket\grandMA3\MA3"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\grandMA3\README_START_HIER.md"; DestDir: "{app}\data\ma-paket\grandMA3"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\grandMA3\REFERENZEN.md"; DestDir: "{app}\data\ma-paket\grandMA3"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\fixtures\MA_OBS_MediaLink_Fade.gdtf"; DestDir: "{app}\data\ma-paket\fixtures"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\fixtures\MA_OBS_MediaLink_Steuerung.gdtf"; DestDir: "{app}\data\ma-paket\fixtures"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\fixtures\README.md"; DestDir: "{app}\data\ma-paket\fixtures"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\OBS\README.md"; DestDir: "{app}\data\ma-paket\OBS"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\docs\WINDOWS_INSTALLER.md"; DestDir: "{app}\data\ma-paket\docs"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\docs\WINDOWS_ZIP.md"; DestDir: "{app}\data\ma-paket\docs"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\docs\BUILD.md"; DestDir: "{app}\data\ma-paket\docs"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\LICENSE"; DestDir: "{app}\data\ma-paket"; Flags: ignoreversion
Source: "{#RepoRoot}\OBS\Release\{#PluginName}\data\ma-paket\THIRD_PARTY.md"; DestDir: "{app}\data\ma-paket"; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion
Source: "{#RepoRoot}\THIRD_PARTY.md"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion
Source: "{#RepoRoot}\CHANGELOG.md"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion
Source: "{#RepoRoot}\docs\WINDOWS_INSTALLER.md"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion
Source: "{#RepoRoot}\dist\BUILD-INFO.json"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion
Source: "{#RepoRoot}\installer\compiler.json"; DestDir: "{app}\medialink-docs"; Flags: ignoreversion

[Registry]
Root: HKLM; Subkey: "Software\MA OBS MediaLink"; ValueType: string; ValueName: "OBSPath"; ValueData: "{code:GetOBSDir}"; Flags: uninsdeletevalue uninsdeletekeyifempty

[Code]
type
  { Inno Setup 6 uses a 32-bit setup process, including on x64 Windows. }
  TProcessEntry = record
    Size, Usage, ProcessID, HeapID, ModuleID, Threads, ParentID: LongWord;
    Priority: Integer;
    Flags: LongWord;
    ExeFile: array[0..259] of Char;
  end;

var
  OBSPage: TInputDirWizardPage;
  DeleteUserSettings: Boolean;

function MediaLinkSettingsPath: String;
begin
  Result := ExpandConstant('{userappdata}\obs-studio\plugin_config\{#PluginName}');
end;

function ChooseSettingsRemoval: Boolean;
var
  Form: TSetupForm;
  Intro, Account, Keep: TNewStaticText;
  Choice: TNewCheckBox;
  ContinueButton, CancelButton: TNewButton;
begin
  DeleteUserSettings := False;
  if UninstallSilent then begin
    DeleteUserSettings := ExpandConstant('{param:DELETESETTINGS|0}') = '1';
    Result := True;
    exit;
  end;
  Form := CreateCustomForm(560, 295, False, True);
  try
    Form.Caption := CustomMessage('RemoveTitle');
    Form.Position := poScreenCenter;
    Intro := TNewStaticText.Create(Form); Intro.Parent := Form;
    Intro.SetBounds(ScaleX(20), ScaleY(16), ScaleX(520), ScaleY(48));
    Intro.AutoSize := False; Intro.WordWrap := True;
    Intro.Caption := CustomMessage('RemoveIntro');
    Choice := TNewCheckBox.Create(Form); Choice.Parent := Form;
    Choice.SetBounds(ScaleX(20), ScaleY(76), ScaleX(520), ScaleY(26));
    Choice.Caption := CustomMessage('DeleteSettings'); Choice.Checked := False;
    Account := TNewStaticText.Create(Form); Account.Parent := Form;
    Account.SetBounds(ScaleX(40), ScaleY(110), ScaleX(500), ScaleY(74));
    Account.AutoSize := False; Account.WordWrap := True;
    Account.Caption := FmtMessage(CustomMessage('SettingsAccount'), [GetUserNameString, MediaLinkSettingsPath]);
    Keep := TNewStaticText.Create(Form); Keep.Parent := Form;
    Keep.SetBounds(ScaleX(20), ScaleY(190), ScaleX(520), ScaleY(42));
    Keep.AutoSize := False; Keep.WordWrap := True; Keep.Caption := CustomMessage('KeepSettings');
    ContinueButton := TNewButton.Create(Form); ContinueButton.Parent := Form;
    ContinueButton.SetBounds(ScaleX(340), ScaleY(250), ScaleX(95), ScaleY(27));
    ContinueButton.Caption := CustomMessage('ContinueRemove');
    ContinueButton.ModalResult := mrOk; ContinueButton.Default := True;
    CancelButton := TNewButton.Create(Form); CancelButton.Parent := Form;
    CancelButton.SetBounds(ScaleX(445), ScaleY(250), ScaleX(95), ScaleY(27));
    CancelButton.Caption := CustomMessage('CancelRemove');
    CancelButton.ModalResult := mrCancel; CancelButton.Cancel := True;
    Result := Form.ShowModal = mrOk;
    if Result then DeleteUserSettings := Choice.Checked;
  finally
    Form.Free;
  end;
end;

function GetBinaryType(FileName: String; var BinaryType: LongWord): Boolean;
  external 'GetBinaryTypeW@kernel32.dll stdcall';
function CreateProcessSnapshot(Flags, ProcessID: LongWord): LongWord;
  external 'CreateToolhelp32Snapshot@kernel32.dll stdcall';
function ProcessFirst(Snapshot: LongWord; var Entry: TProcessEntry): Boolean;
  external 'Process32FirstW@kernel32.dll stdcall';
function ProcessNext(Snapshot: LongWord; var Entry: TProcessEntry): Boolean;
  external 'Process32NextW@kernel32.dll stdcall';
function CloseSnapshot(Handle: LongWord): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function GetOBSDir(Param: String): String;
begin
  Result := RemoveBackslashUnlessRoot(Trim(OBSPage.Values[0]));
end;

function DetectOBS: String;
begin
  Result := '';
  if RegQueryStringValue(HKLM64, 'Software\MA OBS MediaLink', 'OBSPath', Result) then
    if FileExists(AddBackslash(Result) + 'bin\64bit\obs64.exe') then exit;
  if RegQueryStringValue(HKLM64, 'Software\OBS Studio', '', Result) then
    if FileExists(AddBackslash(Result) + 'bin\64bit\obs64.exe') then exit;
  if RegQueryStringValue(HKLM32, 'Software\OBS Studio', '', Result) then
    if FileExists(AddBackslash(Result) + 'bin\64bit\obs64.exe') then exit;
  Result := ExpandConstant('{autopf64}\obs-studio');
end;

function CheckOBSFolder: String;
var
  Folder, ObsExe, Duplicate: String;
  VersionMS, VersionLS, BinaryType: Cardinal;
begin
  Result := CustomMessage('InvalidOBS');
  Folder := AddBackslash(GetOBSDir(''));
  ObsExe := Folder + 'bin\64bit\obs64.exe';
  if not FileExists(ObsExe) then exit;
  if not GetBinaryType(ObsExe, BinaryType) then exit;
  if BinaryType <> 6 then exit; { SCS_64BIT_BINARY }
  if not GetVersionNumbers(ObsExe, VersionMS, VersionLS) then exit;
  if (VersionMS shr 16) <> 33 then exit;
  if FileExists(Folder + 'portable_mode') or FileExists(Folder + 'portable_mode.txt') or
     FileExists(Folder + 'obs_portable_mode') or FileExists(Folder + 'obs_portable_mode.txt') then begin
    Result := CustomMessage('PortableOBS');
    exit;
  end;
  Duplicate := Folder + 'obs-plugins\64bit\{#PluginName}.dll';
  if FileExists(Duplicate) then begin
    Result := FmtMessage(CustomMessage('DuplicateOBS'), [Duplicate]);
    exit;
  end;
  Result := '';
end;

function CheckOBSStopped: String;
var
  Snapshot: LongWord;
  Entry: TProcessEntry;
  More: Boolean;
  Name: String;
  I: Integer;
begin
  Result := CustomMessage('ProcessError');
  Snapshot := CreateProcessSnapshot(2, 0); { TH32CS_SNAPPROCESS }
  if Snapshot = $FFFFFFFF then exit;
  try
    Entry.Size := 556; { sizeof(PROCESSENTRY32W) in a 32-bit process }
    More := ProcessFirst(Snapshot, Entry);
    while More do begin
      Name := '';
      for I := 0 to 259 do begin
        if Entry.ExeFile[I] = #0 then break;
        Name := Name + Entry.ExeFile[I];
      end;
      if CompareText(Name, 'obs64.exe') = 0 then begin
        Result := CustomMessage('RunningOBS');
        exit;
      end;
      More := ProcessNext(Snapshot, Entry);
    end;
    if DLLGetLastError = 18 then Result := ''; { ERROR_NO_MORE_FILES }
  finally
    CloseSnapshot(Snapshot);
  end;
end;

procedure InitializeWizard;
begin
  OBSPage := CreateInputDirPage(wpLicense, CustomMessage('OBSPage'),
    CustomMessage('OBSDescription'), CustomMessage('OBSHelp'), False, '');
  OBSPage.Add(CustomMessage('OBSFolder'));
  OBSPage.Values[0] := ExpandConstant('{param:OBSDIR|' + DetectOBS + '}');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Error: String;
begin
  Result := True;
  if CurPageID = OBSPage.ID then begin
    Error := CheckOBSFolder;
    Result := Error = '';
    if not Result then begin
      Log(Error);
      SuppressibleMsgBox(Error, mbError, MB_OK, IDOK);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := CheckOBSFolder;
  if Result = '' then Result := CheckOBSStopped;
end;

function InitializeUninstall: Boolean;
var
  Error: String;
begin
  Error := CheckOBSStopped;
  Result := Error = '';
  if not Result then begin
    Log(Error);
    SuppressibleMsgBox(Error, mbError, MB_OK, IDOK);
  end;
  if Result then Result := ChooseSettingsRemoval;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Error: String;
begin
  if CurUninstallStep = usUninstall then begin
    Error := CheckOBSStopped;
    if Error <> '' then RaiseException(Error);
  end;
  if (CurUninstallStep = usPostUninstall) and DeleteUserSettings and DirExists(MediaLinkSettingsPath) then begin
    Error := CheckOBSStopped;
    if Error = '' then
      if not DelTree(MediaLinkSettingsPath, True, True, True) then Error := 'Settings folder could not be removed';
    if Error <> '' then begin
      Log('MediaLink settings removal incomplete: ' + MediaLinkSettingsPath);
      SuppressibleMsgBox(FmtMessage(CustomMessage('SettingsError'), [MediaLinkSettingsPath]), mbError, MB_OK, IDOK);
    end else Log('Deleted MediaLink settings: ' + MediaLinkSettingsPath);
  end;
end;
