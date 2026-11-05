# Automatischer Windows-Build

## Auf GitHub starten

1. Die geänderten Dateien einschließlich `.github/workflows/windows-build.yml` in das bestehende Repository hochladen und den Commit speichern. Die Ordnerstruktur muss erhalten bleiben.
2. Im Repository **Actions → Tests und Windows-Build** öffnen. Der Upload startet den Ablauf automatisch. Falls GitHub zunächst das Aktivieren von Actions verlangt, diese aktivieren. Danach lässt sich der Ablauf über **Run workflow** auf dem Standardbranch erneut starten.
3. Erst läuft **Steuerungs- und MA-Tests**, dann **Windows x64 / OBS 33**. Beim ersten Durchlauf werden größere Abhängigkeiten heruntergeladen und die OBS-Bibliotheken gebaut. Das kann mehrere Minuten dauern.
4. Nach einem grünen Durchlauf unten unter **Artifacts** das Paket **MA-OBS-MediaLink-v046-MA042-windows-x64** herunterladen. Zum Download bei GitHub angemeldet bleiben. Die Bereitstellung ist auf 30 Tage begrenzt; ein späterer Build erzeugt sie erneut.

Das heruntergeladene GitHub-Archiv enthält:

| Datei | Inhalt |
| --- | --- |
| `MA_OBS_MediaLink_v046_Setup_x64.exe` | Windows-Installer mit Update und Deinstallation |
| `MA_OBS_MediaLink_v046_Windows_x64.zip` | Fertige Plugin-DLL, Shader, Hinweise und Anleitung |
| `MA_OBS_MediaLink_MA042.zip` | grandMA3-Importdateien, beide GDTF-Profile und MA-Anleitungen |
| `SHA256SUMS.txt` | SHA256-Prüfsummen von Setup und beiden ZIP-Dateien |

Die Pakete werden auch bei Pull Requests zum Prüfen erzeugt. Es wird kein Release veröffentlicht und nichts auf einem Nutzerrechner installiert. Eigene Repository-Secrets sind nicht erforderlich; der Ablauf hat nur Leserechte für den Repository-Inhalt.

Bei einem roten Durchlauf den fehlgeschlagenen Schritt öffnen und dessen Fehlermeldung weitergeben. Solange die Tests scheitern, wird kein Windows-Paket gebaut. Ein erfolgreicher Build bestätigt noch keine Laufzeitkompatibilität mit OBS, Blackmagic oder grandMA3.

## Festgelegte Abhängigkeiten

`OBS/dependencies.json` legt OBS **33.0.0**, die OBS-Abhängigkeiten vom **23.10.2026**, Qt **6.11.1** und die SHA256-Werte der vier Archive fest. Das kleine x86-Archiv ist für OBS-interne Capture-Hilfsziele erforderlich; MediaLink selbst wird ausschließlich für x64 gebaut. Der Build verwendet den GitHub-Runner `windows-2025-vs2026` und den CMake-Generator **Visual Studio 18 2026**. Änderungen an diesen Versionen erfolgen bewusst in der Datei; es wird nicht automatisch die neueste OBS-Version verwendet.

Die Abhängigkeiten entsprechen den [offiziellen OBS-33-Vorgaben](https://github.com/obsproject/obs-studio/blob/33.0.0/CMakePresets.json). Das Erzeugen eines Development-SDKs aus `libobs` und `obs-frontend-api` orientiert sich am [offiziellen Plugin-Template](https://github.com/obsproject/obs-plugintemplate/blob/master/cmake/common/buildspec_common.cmake).

GitHub speichert nur die heruntergeladenen Archive zwischen. Sie werden vor jeder Verwendung anhand der Prüfsummen geprüft. Das SDK wird auf dem jeweiligen Runner frisch erzeugt. Die Runner-Werkzeuge und Betriebssystemaktualisierungen werden von GitHub gepflegt; der Ablauf garantiert keine bytegleichen DLLs bei späteren Läufen.

## Lokal bauen

Voraussetzungen: Windows x64, Visual Studio 2026 mit **Desktopentwicklung mit C++** und Windows-SDK, CMake ab 3.28, `curl.exe`; für die ZIP-Erzeugung zusätzlich Python 3.12. OBS muss zum Bauen nicht installiert sein. Wegen der Windows-Pfadlängengrenze einen kurzen Entpackpfad verwenden, beispielsweise `C:\OBS-Development\MA-OBS-MediaLink`. PowerShell im Repository-Hauptordner öffnen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\OBS\Build_OBS33_Release.ps1
python -X utf8 .\tools\build_packages.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\installer\Build_Installer.ps1
python -X utf8 .\tools\build_packages.py --finalize-installer
```

Die Befehle bauen das OBS-SDK und Plugin, erzeugen die ZIPs, bauen das Setup und ergänzen seine Prüfsumme unter `dist/`. Der Installer-Build lädt Inno Setup 6.7.4 mit der in `installer/compiler.json` festgelegten Prüfsumme und entpackt den Compiler im offiziellen portablen Modus nach `installer/.deps`. Er installiert den Compiler nicht systemweit. Die erzeugten Ordner werden nicht ins Repository hochgeladen.

Ein bereits vorhandenes SDK aus dem bisherigen Plugin-Template kann ausdrücklich weiterverwendet werden:

```powershell
.\OBS\Build_OBS33_Release.ps1 -UseExistingDeps -DepsRoot 'C:\OBS-Development\obs-plugintemplate-33\.deps'
```

Dieser Kompatibilitätsmodus prüft keine heruntergeladenen Archive und wird in GitHub Actions nicht verwendet. Der normale Ablauf verwendet einen eigenen Ordner `OBS/.deps`. Ein anderer Speicherort lässt sich über `-DepsRoot` angeben. Die vorbereiteten Ordner sind an ihren Buildpfad und Generator gebunden; für einen Wechsel einen neuen Ordner verwenden.

## Tests

Auf Ubuntu 24.04 mit Python 3.12, `g++` und `liblua5.4-0`:

```sh
python tools/run_tests.py
```

Der Aufruf führt alle dreizehn mitgelieferten OBS-Testskripte aus, danach die MA-Tests und den Abgleich des eingebetteten Lua-Kerns mit der Importdatei. Die C++-Tests verwenden ihre vorhandenen OBS-/Qt-Testadapter und überwiegend AddressSanitizer/UndefinedBehaviorSanitizer. Der zusätzliche Verbindungstest prüft Universum 1, automatische Multicast-Vorgabe und den Erhalt gespeicherter Verbindungswerte. Das vollständige Plugin wird separat unter Windows kompiliert.

`installer/Test_Installer.ps1` führt auf dem frischen GitHub-Windows-Runner die echte Setup-Datei aus. Ein künstliches x64-Programm mit OBS-33-Dateiversion prüft Ordnererkennung, portable Markierung, doppelte DLL, laufenden Prozess, Erstinstallation, MA-Paket-Update, Deinstallation und Neuinstallation mit erhaltenen sowie ausdrücklich gelöschten Einstellungen. Fremde Dateien, andere Plugin-Konfigurationen und Verzeichnisverknüpfungs-Ziele müssen erhalten bleiben. Im Projekt-Repository lädt der Workflow zusätzlich den veröffentlichten v0.45-Installer mit festgelegter Prüfsumme und prüft das echte Versionsupdate auf v0.46. Das Testskript verweigert die Ausführung auf einem normalen Arbeitsplatz oder einem Runner mit vorhandenem OBS/MediaLink. Es ersetzt keine Tests mit echtem OBS.

Ich habe den bisherigen Praxistest mit OBS 33.0.1 und grandMA3 2.5.0.3 sowie die Installation auf einem zweiten Rechner ohne Entwicklerwerkzeuge erfolgreich durchgeführt. Auch die erneute Installation desselben Pakets und die bisherigen Deinstallations-/Einstellungsprüfungen waren erfolgreich. Ein Update von jeder älteren Pluginversion habe ich nicht einzeln getestet. Meine Tests gelten für den verwendeten Aufbau; weitere Versionen und Hardwarekombinationen müssen separat geprüft werden. Anleitung für das Setup: [WINDOWS_INSTALLER.md](WINDOWS_INSTALLER.md). Vorbereitung der geprüften Beta-Dateien: [BETA_RELEASE.md](BETA_RELEASE.md).
