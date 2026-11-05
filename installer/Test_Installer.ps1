param([string]$PreviousSetup = '')
# Disposable GitHub runner only: registers the real setup and then removes it.
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) {
    throw 'Diesen Test nur auf einem frischen GitHub-Windows-Runner ausfuehren.'
}
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Der Installertest benoetigt einen administrativen Test-Runner.'
}
if ((Test-Path 'HKLM:\SOFTWARE\OBS Studio') -or
    (Test-Path 'HKLM:\SOFTWARE\MA OBS MediaLink') -or
    (Get-Process obs64 -ErrorAction SilentlyContinue)) {
    throw 'Auf diesem Rechner sind OBS oder MediaLink vorhanden. Test abgebrochen.'
}
$repo = Split-Path $PSScriptRoot -Parent
$setup = Join-Path $repo 'dist\MA_OBS_MediaLink_v046_Setup_x64.exe'
$testRoot = Join-Path $env:RUNNER_TEMP ('medialink-installer-' + [guid]::NewGuid().ToString('N'))
$checkRegistration = $true
$testJunction = $true
$testProfileRoot = Join-Path $env:APPDATA 'obs-studio\plugin_config'
$obsDir = Join-Path $testRoot 'OBS fixture'
$settingsPath = Join-Path $testProfileRoot 'obs-sacn-crossfade'
if (Test-Path -LiteralPath $settingsPath) { throw 'Vorhandene MediaLink-Einstellungen: Installertest abgebrochen.' }
$otherSettingsPath = Join-Path $testProfileRoot ('installer-test-other-' + [guid]::NewGuid().ToString('N'))
$target = Join-Path $testRoot 'Plugin target'
$obsBin = Join-Path $obsDir 'bin\64bit'
New-Item -ItemType Directory -Path $obsBin -Force | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$source = Join-Path $testRoot 'obs-fixture.cs'
@'
using System.Reflection;
[assembly: AssemblyFileVersion("33.0.0.0")]
public class ObsFixture {
    public static void Main() { System.Threading.Thread.Sleep(120000); }
}
'@ | Set-Content -LiteralPath $source -Encoding utf8
$obsExe = Join-Path $obsBin 'obs64.exe'
& $csc /nologo /target:exe /platform:x64 ('/out:' + $obsExe) $source
if ($LASTEXITCODE -ne 0) { throw 'OBS-Testprogramm konnte nicht gebaut werden.' }

function Invoke-Installer([string]$File, [string[]]$Extra, [bool]$ExpectSuccess, [string]$Name) {
    $log = Join-Path $testRoot ($Name + '.log')
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/LANG=english', ('/LOG="' + $log + '"')) + $Extra
    # -Wait includes the uninstaller's temporary child process. Waiting only
    # for its launcher can race with reinstallation and create unins001.exe.
    $process = Start-Process -FilePath $File -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
    if (($process.ExitCode -eq 0) -ne $ExpectSuccess) {
        if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 35 }
        throw "$Name unerwarteter Exitcode: $($process.ExitCode)"
    }
    $expectedErrors = @{
        'missing-obs' = 'does not contain OBS Studio 33'
        'wrong-obs-version' = 'does not contain OBS Studio 33'
        'wrong-obs-architecture' = 'does not contain OBS Studio 33'
        'portable-obs' = 'marked as portable'
        'duplicate-plugin' = 'already exists in another OBS plugin folder'
        'running-obs-install' = 'OBS is still running'
        'running-obs-uninstall' = 'OBS is still running'
    }
    if ($expectedErrors.ContainsKey($Name)) {
        if (-not (Get-Content -LiteralPath $log -Raw).Contains($expectedErrors[$Name])) {
            throw "$Name wurde aus einem unerwarteten Grund abgebrochen. Log: $log"
        }
    }
    Write-Host "OK: $Name"
}
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Installed {
    $installed = Join-Path $target 'bin\64bit\obs-sacn-crossfade.dll'
    $original = Join-Path $repo 'OBS\Release\obs-sacn-crossfade\bin\64bit\obs-sacn-crossfade.dll'
    Assert-True (Test-Path -LiteralPath $installed) 'Installierte DLL fehlt.'
    Assert-True ((Get-FileHash -LiteralPath $installed).Hash -eq (Get-FileHash -LiteralPath $original).Hash) 'DLL wurde nicht korrekt installiert.'
    Assert-True (Test-Path -LiteralPath (Join-Path $target 'data\crossfade.effect')) 'Shader fehlt.'
    $package = Join-Path $target 'data\ma-paket'
    foreach ($relative in @('START_HIER.html', 'grandMA3\MA3\MA_OBS_MediaLink.xml',
        'grandMA3\MA3\MA_OBS_MediaLink.lua', 'grandMA3\README_START_HIER.md',
        'grandMA3\REFERENZEN.md', 'fixtures\MA_OBS_MediaLink_Fade.gdtf',
        'fixtures\MA_OBS_MediaLink_Steuerung.gdtf', 'fixtures\README.md',
        'OBS\README.md', 'docs\WINDOWS_INSTALLER.md', 'docs\WINDOWS_ZIP.md', 'docs\BUILD.md', 'LICENSE', 'THIRD_PARTY.md')) {
        $expected = Join-Path $repo ('OBS\Release\obs-sacn-crossfade\data\ma-paket\' + $relative)
        $actual = Join-Path $package $relative
        Assert-True (Test-Path -LiteralPath $actual) ('MA-Paket unvollstaendig: ' + $relative)
        Assert-True ((Get-FileHash -LiteralPath $actual).Hash -eq (Get-FileHash -LiteralPath $expected).Hash) ('Falsche MA-Datei: ' + $relative)
    }
    Assert-True (Test-Path -LiteralPath (Join-Path $target 'data\medialink.ico')) 'Logo fuer Deinstallation fehlt.'
    if ($checkRegistration) {
        $registered = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\MA OBS MediaLink'
        Assert-True ($registered.OBSPath -eq $obsDir) 'OBS-Pfad wurde nicht registriert.'
        Assert-True (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MA_OBS_MediaLink_OBS_x64_is1') 'Windows-Deinstallationsregistrierung fehlt.'
        $uninstallEntry = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MA_OBS_MediaLink_OBS_x64_is1'
        Assert-True ($uninstallEntry.DisplayIcon -like '*medialink.ico*') 'Windows-Deinstallationseintrag verwendet nicht das Projektlogo.'
    }
}
$installArgs = @(('/DIR="' + $target + '"'), ('/OBSDIR="' + $obsDir + '"'))
$uninstall = Join-Path $target 'uninstall\unins000.exe'
$fakeProcess = $null
try {
    Invoke-Installer $setup @(('/DIR="' + $target + '"'), ('/OBSDIR="' + $testRoot + '"')) $false 'missing-obs'
    Assert-True (-not (Test-Path -LiteralPath $target)) 'Fehlgeschlagener Test hat Dateien installiert.'
    $validSource = Get-Content -LiteralPath $source -Raw
    Set-Content -LiteralPath $source -Value $validSource.Replace('33.0.0.0', '32.0.0.0') -Encoding utf8
    & $csc /nologo /target:exe /platform:x64 ('/out:' + $obsExe) $source
    if ($LASTEXITCODE -ne 0) { throw 'Falsche Versions-Fixture konnte nicht gebaut werden.' }
    Invoke-Installer $setup $installArgs $false 'wrong-obs-version'
    Set-Content -LiteralPath $source -Value $validSource -Encoding utf8
    & $csc /nologo /target:exe /platform:x86 ('/out:' + $obsExe) $source
    if ($LASTEXITCODE -ne 0) { throw 'x86-Fixture konnte nicht gebaut werden.' }
    Invoke-Installer $setup $installArgs $false 'wrong-obs-architecture'
    & $csc /nologo /target:exe /platform:x64 ('/out:' + $obsExe) $source
    if ($LASTEXITCODE -ne 0) { throw 'Gueltige OBS-Fixture konnte nicht wiederhergestellt werden.' }
    $portable = Join-Path $obsDir 'portable_mode.txt'
    Set-Content -LiteralPath $portable -Value ''
    Invoke-Installer $setup $installArgs $false 'portable-obs'
    Remove-Item -LiteralPath $portable
    $duplicate = Join-Path $obsDir 'obs-plugins\64bit\obs-sacn-crossfade.dll'
    New-Item -ItemType Directory -Path (Split-Path $duplicate) -Force | Out-Null
    Set-Content -LiteralPath $duplicate -Value 'duplicate marker'
    Invoke-Installer $setup $installArgs $false 'duplicate-plugin'
    Remove-Item -LiteralPath $duplicate
    $fakeProcess = Start-Process -FilePath $obsExe -WindowStyle Hidden -PassThru
    Invoke-Installer $setup $installArgs $false 'running-obs-install'
    Assert-True (-not $fakeProcess.HasExited) 'Setup hat das OBS-Testprogramm beendet.'
    Stop-Process -Id $fakeProcess.Id
    $fakeProcess.WaitForExit()
    $fakeProcess = $null
    Invoke-Installer $setup $installArgs $true 'first-install'
    Assert-Installed
    New-Item -ItemType Directory -Path (Join-Path $settingsPath 'video_rules'), $otherSettingsPath -Force | Out-Null
    $configFile = Join-Path $settingsPath 'native_settings.json'
    Set-Content -LiteralPath $configFile -Value '{"universe":12,"multicast_interface":"10.20.30.40"}'
    Set-Content -LiteralPath (Join-Path $settingsPath 'video_rules\collection.json') -Value '{"preserve":true}'
    $otherFile = Join-Path $otherSettingsPath 'keep.json'
    Set-Content -LiteralPath $otherFile -Value '{"otherPlugin":true}'
    $configHash = (Get-FileHash -LiteralPath $configFile).Hash
    $otherHash = (Get-FileHash -LiteralPath $otherFile).Hash
    $sentinel = Join-Path $target 'data\user-settings-sentinel.json'
    Set-Content -LiteralPath $sentinel -Value '{"keep":true}'
    $sentinelHash = (Get-FileHash -LiteralPath $sentinel).Hash
    Set-Content -LiteralPath (Join-Path $target 'bin\64bit\obs-sacn-crossfade.dll') -Value 'old DLL marker'
    Set-Content -LiteralPath (Join-Path $target 'data\ma-paket\grandMA3\MA3\MA_OBS_MediaLink.lua') -Value 'old MA plugin marker'
    Invoke-Installer $setup $installArgs $true 'update'
    Assert-Installed
    Assert-True ((Get-FileHash -LiteralPath $sentinel).Hash -eq $sentinelHash) 'Update hat fremde Einstellungen veraendert.'
    Assert-True ((Get-FileHash -LiteralPath $configFile).Hash -eq $configHash) 'Update hat MediaLink-Einstellungen veraendert.'
    $fakeProcess = Start-Process -FilePath $obsExe -WindowStyle Hidden -PassThru
    Invoke-Installer $uninstall @() $false 'running-obs-uninstall'
    Assert-Installed
    Assert-True (-not $fakeProcess.HasExited) 'Deinstaller hat das OBS-Testprogramm beendet.'
    Stop-Process -Id $fakeProcess.Id
    $fakeProcess.WaitForExit()
    $fakeProcess = $null
    Invoke-Installer $uninstall @() $true 'uninstall'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $target 'bin\64bit\obs-sacn-crossfade.dll'))) 'Deinstallation hat DLL zurueckgelassen.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $target 'data\crossfade.effect'))) 'Deinstallation hat Shader zurueckgelassen.'
    Assert-True ((Get-FileHash -LiteralPath $sentinel).Hash -eq $sentinelHash) 'Deinstallation hat fremde Einstellungen entfernt.'
    Assert-True ((Get-FileHash -LiteralPath $configFile).Hash -eq $configHash) 'Standard-Deinstallation hat Einstellungen entfernt.'
    if ($checkRegistration) {
        Assert-True (-not (Test-Path 'HKLM:\SOFTWARE\MA OBS MediaLink')) 'OBS-Pfadregistrierung wurde nicht entfernt.'
        Assert-True (-not (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MA_OBS_MediaLink_OBS_x64_is1')) 'Windows-Deinstallationsregistrierung wurde nicht entfernt.'
    }
    Invoke-Installer $setup $installArgs $true 'reinstall'
    Assert-Installed
    Assert-True ((Get-FileHash -LiteralPath $configFile).Hash -eq $configHash) 'Neuinstallation hat Einstellungen ersetzt.'
    $external = Join-Path $testRoot 'external-sentinel'
    New-Item -ItemType Directory -Path $external -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $external 'keep.json') -Value '{"outside":true}'
    if ($testJunction) {
        New-Item -ItemType Junction -Path (Join-Path $settingsPath 'linked-external') -Target $external | Out-Null
    }
    Invoke-Installer $uninstall @('/DELETESETTINGS=1') $true 'uninstall-delete-settings'
    Assert-True (-not (Test-Path -LiteralPath $settingsPath)) 'Ausgewaehlte Einstellungen wurden nicht geloescht.'
    Assert-True (Test-Path -LiteralPath (Join-Path $external 'keep.json')) 'Deinstaller hat ein Junction-Ziel geloescht.'
    Assert-True ((Get-FileHash -LiteralPath $otherFile).Hash -eq $otherHash) 'Einstellungen anderer Plugins wurden veraendert.'
    Assert-True ((Get-FileHash -LiteralPath $sentinel).Hash -eq $sentinelHash) 'Fremde Programmordner-Datei wurde entfernt.'
    Invoke-Installer $setup $installArgs $true 'fresh-reinstall-after-purge'
    Assert-Installed
    Assert-True (-not (Test-Path -LiteralPath $settingsPath)) 'Setup hat geloeschte Einstellungen wiederhergestellt.'
    Invoke-Installer $uninstall @() $true 'final-uninstall'
    Assert-True ((Get-FileHash -LiteralPath $otherFile).Hash -eq $otherHash) 'Abschliessende Deinstallation hat andere Einstellungen veraendert.'
    if ($PreviousSetup) {
        if ((Get-FileHash -LiteralPath $PreviousSetup -Algorithm SHA256).Hash -ne 'de297e64cb00fa78daf48fe3e88c870e9e160fc1916f80b4fff8a9dc892d78b2') {
            throw 'Der bisherige Installer entspricht nicht der veroeffentlichten v0.45-Beta.'
        }
        Invoke-Installer $PreviousSetup $installArgs $true 'install-released-v045'
        New-Item -ItemType Directory -Path $settingsPath -Force | Out-Null
        Set-Content -LiteralPath $configFile -Value '{"universe":27,"multicast_interface":"10.55.66.77"}'
        $oldConfigHash = (Get-FileHash -LiteralPath $configFile).Hash
        Invoke-Installer $setup $installArgs $true 'upgrade-v045-to-v046'
        Assert-Installed
        Assert-True ((Get-FileHash -LiteralPath $configFile).Hash -eq $oldConfigHash) 'Versionsupdate hat vorhandene Verbindungswerte veraendert.'
        Invoke-Installer $uninstall @('/DELETESETTINGS=1') $true 'uninstall-upgraded-version'
        Assert-True (-not (Test-Path -LiteralPath $settingsPath)) 'Einstellungen der aktualisierten Installation wurden nicht entfernt.'
        Assert-True ((Get-FileHash -LiteralPath $otherFile).Hash -eq $otherHash) 'Versionsupdate hat fremde Konfiguration veraendert.'
    }
    Write-Host "Alle Installertests erfolgreich. Logs: $testRoot"
} finally {
    if ($fakeProcess -and -not $fakeProcess.HasExited) { Stop-Process -Id $fakeProcess.Id }
}
