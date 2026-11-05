# MA OBS MediaLink v0.45 BETA · Windows-Setup

Der Installer ist für eine normale Installation von **OBS Studio 33 unter Windows x64** bestimmt. Er benötigt Administratorrechte. Portable OBS-Installationen und Windows ARM64 werden von diesem Setup nicht unterstützt.

## Installieren und aktualisieren

1. Vor dem ersten Test die OBS-Konfiguration und bei einem Update den bisherigen Pluginordner sichern.
2. OBS vollständig schließen, auch auf weiteren angemeldeten Windows-Sitzungen.
3. **MA_OBS_MediaLink_v045_Setup_x64.exe** starten. Den erkannten OBS-Ordner prüfen und bei Bedarf ändern. Gemeint ist der Hauptordner, zum Beispiel `C:\Program Files\obs-studio`.
4. Installation abschließen, OBS starten und **Werkzeuge → MA OBS MediaLink** öffnen.

Das Setup installiert DLL und Shader nach `%ProgramData%\obs-studio\plugins\obs-sacn-crossfade`. Bei einem Update ersetzt es diese Dateien. Eine dort bereits manuell installierte Version wird am selben Ort aktualisiert. Eine zusätzliche Crossfade-Quelle ist nicht erforderlich. Das passende **MA_OBS_MediaLink_MA042.zip** enthält grandMA3-Plugin und Fixtureprofile und wird separat importiert.

Wird eine weitere MediaLink-DLL unter `obs-plugins\64bit` im ausgewählten OBS-Ordner gefunden, hält das Setup an. Diese Kopie zuerst sichern und entfernen. Weitere selbst gewählte Pluginpfade ebenfalls prüfen, damit OBS das Plugin nur einmal lädt. OBS muss auch beim Update geschlossen bleiben; das Setup beendet OBS nicht automatisch.

## Deinstallieren

OBS schließen. Unter **Windows-Einstellungen → Apps → Installierte Apps → MA OBS MediaLink** die Deinstallation starten. Der Deinstaller entfernt die vom Setup verwalteten Dateien. Er löscht keine OBS-Konfiguration und keine MediaLink-Regeln. Unbekannte Dateien im Pluginordner bleiben erhalten.

Die Einstellungen liegen getrennt von den Programmdateien in der OBS-Konfiguration, normalerweise unter `%APPDATA%\obs-studio\plugin_config\obs-sacn-crossfade`. Eine Neuinstallation kann sie wiederverwenden. Bei einer früheren manuellen Installation an einem anderen Ort muss diese Kopie separat entfernt werden.

## Herkunft und Teststand

Das Setup enthält keine OBS-, Qt- oder SDK-Pakete. Seine Plugin-DLL entspricht der DLL im Windows-ZIP desselben Builds. Lizenz, Drittanbieterhinweise und `BUILD-INFO.json` liegen nach der Installation unter `medialink-docs` im Pluginordner. Die Prüfsumme des Setups steht in der mitgelieferten `SHA256SUMS.txt`.

Das Setup ist noch nicht mit einem Herausgeberzertifikat signiert. Windows kann daher beim ersten Start einen Hinweis auf den unbekannten Herausgeber anzeigen. Vor einer öffentlichen Weitergabe müssen Quellstand und Lizenzinformationen zugänglich sein.

Der automatisierte Installertest prüft Installation, erneute Installation, Abbruch bei laufendem OBS, Deinstallation und Erhalt fremder Dateien mit einem künstlichen OBS-Testprogramm. Er bestätigt keine OBS-Laufzeitkompatibilität. Vor der Beta-Veröffentlichung bleiben der Test ohne Entwicklerwerkzeuge, echte OBS-Updates, Audio, Blackmagic, OBS-Shutdown und die grandMA3-Fixtureimporte erforderlich.

## Unbeaufsichtigte Installation

Für Testrechner oder verwaltete Installationen:

```powershell
.\MA_OBS_MediaLink_v045_Setup_x64.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /OBSDIR="C:\Program Files\obs-studio" /LOG="C:\Temp\MediaLink-Setup.log"
```

Auch dabei gelten OBS-Versionsprüfung und Sperre bei laufendem OBS. Ein fehlgeschlagener Vorgang liefert einen Rückgabewert ungleich null.
