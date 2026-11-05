# MA OBS MediaLink v0.46 BETA · Windows-Setup

Der Installer ist für eine normale Installation von **OBS Studio 33 unter Windows x64** bestimmt. Er benötigt Administratorrechte. Portable OBS-Installationen und Windows ARM64 werden von diesem Setup nicht unterstützt.

## Installieren und aktualisieren

1. Vor dem ersten Test die OBS-Konfiguration und bei einem Update den bisherigen Pluginordner sichern.
2. OBS vollständig schließen, auch auf weiteren angemeldeten Windows-Sitzungen.
3. **MA_OBS_MediaLink_v046_Setup_x64.exe** starten. Den erkannten OBS-Ordner prüfen und bei Bedarf ändern. Gemeint ist der Hauptordner, zum Beispiel `C:\Program Files\obs-studio`.
4. Installation abschließen, OBS starten und **Werkzeuge → MA OBS MediaLink** öffnen.

Das Setup installiert DLL, Shader, Logo und das lokale MA-Paket nach `%ProgramData%\obs-studio\plugins\obs-sacn-crossfade`. Bei einem Update ersetzt es die mitgelieferten Dateien am gleichen Ort. Eine dort bereits manuell installierte Version wird aktualisiert; gespeicherte Universen, IP-Adressen und Regeln bleiben erhalten. Eine zusätzliche Crossfade-Quelle ist nicht erforderlich.

Unter **Allgemein → MA-Paket und Anleitungen öffnen** öffnest du den Ordner `data\ma-paket`. Die Datei **START_HIER.html** enthält eine offline nutzbare Anleitung. XML und Lua des MA-Plugins sowie beide GDTF-Fixtures liegen dort ebenfalls bereit. Updates ersetzen die mitgelieferten MA-Dateien. Den Import in grandMA3 führst du weiterhin selbst aus. Das separate **MA_OBS_MediaLink_MA042.zip** bleibt zusätzlich als Download verfügbar.

Bei einer frischen Einrichtung gelten **Universum 1** und **Automatisch (Windows-Standard)** für die Multicast-Schnittstelle. Bei mehreren Netzwerkkarten kannst du eine konkrete lokale IPv4-Schnittstelle wählen. Vorhandene Einstellungen aus v0.45 werden nicht automatisch auf diese neuen Vorgaben zurückgesetzt.

Wird eine weitere MediaLink-DLL unter `obs-plugins\64bit` im ausgewählten OBS-Ordner gefunden, hält das Setup an. Diese Kopie zuerst sichern und entfernen. Weitere selbst gewählte Pluginpfade ebenfalls prüfen, damit OBS das Plugin nur einmal lädt. OBS muss auch beim Update geschlossen bleiben; das Setup beendet OBS nicht automatisch.

## Deinstallieren

OBS schließen. Unter **Windows-Einstellungen → Apps → Installierte Apps → MA OBS MediaLink** die Deinstallation starten. In der zusätzlichen Abfrage ist **MediaLink-Einstellungen ebenfalls löschen** standardmäßig ausgeschaltet:

- **Ausgeschaltet:** MediaLink-Regeln und Einstellungen bleiben für eine Neuinstallation erhalten.
- **Eingeschaltet:** Der angezeigte MediaLink-Einstellungsordner einschließlich Regeln und Sicherungsdateien wird nach der Deinstallation entfernt.

Der Dialog nennt Windows-Konto und vollständigen Ordner. Die Auswahl gilt nur für dieses Konto; weitere Benutzerprofile werden nicht durchsucht. Wenn du die Deinstallation mit den Zugangsdaten eines anderen Administratorkontos startest, prüfe das angezeigte Konto und den Pfad besonders: Die Einstellungen deines normalen Kontos können an einem anderen Ort liegen. OBS-Szenen, Profile, Einstellungen anderer Plugins und unbekannte Dateien im Programmordner bleiben erhalten.

Die Einstellungen liegen getrennt von den Programmdateien in der OBS-Konfiguration, normalerweise unter `%APPDATA%\obs-studio\plugin_config\obs-sacn-crossfade`. Eine Neuinstallation kann sie wiederverwenden. Bei einer früheren manuellen Installation an einem anderen Ort muss diese Kopie separat entfernt werden.

## Herkunft und Teststand

Das Setup enthält keine OBS-, Qt- oder SDK-Pakete. Seine Plugin-DLL entspricht der DLL im Windows-ZIP desselben Builds. Lizenz, Drittanbieterhinweise und `BUILD-INFO.json` liegen nach der Installation unter `medialink-docs` im Pluginordner. Die Prüfsumme des Setups steht in der mitgelieferten `SHA256SUMS.txt`.

Das Setup ist noch nicht mit einem Herausgeberzertifikat signiert. Windows kann daher beim ersten Start einen Hinweis auf den unbekannten Herausgeber anzeigen. Vor einer öffentlichen Weitergabe müssen Quellstand und Lizenzinformationen zugänglich sein.

Der automatisierte Installertest prüft Installation, MA-Paket-Updates, Abbruch bei laufendem OBS, Behalten/Löschen der Einstellungen, Neuinstallation und den Erhalt fremder Dateien mit einem künstlichen OBS-Testprogramm. Im Projekt-Workflow wird zusätzlich die veröffentlichte v0.45 installiert und auf v0.46 aktualisiert. Das ersetzt nicht den Praxistest in echtem OBS. Die manuelle Prüfung der neuen Bedienelemente, der Multicast-Auswahl und der neuen Deinstallationsabfrage steht für v0.46 vor einer Veröffentlichung noch an.

## Unbeaufsichtigte Installation

Für Testrechner oder verwaltete Installationen:

```powershell
.\MA_OBS_MediaLink_v046_Setup_x64.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /OBSDIR="C:\Program Files\obs-studio" /LOG="C:\Temp\MediaLink-Setup.log"
```

Auch dabei gelten OBS-Versionsprüfung und Sperre bei laufendem OBS. Ein fehlgeschlagener Vorgang liefert einen Rückgabewert ungleich null.

Eine unbeaufsichtigte Deinstallation behält die Einstellungen standardmäßig bei. Nur der ausdrücklich übergebene Parameter `/DELETESETTINGS=1` wählt deren Löschung im Profil des ausführenden Kontos aus.
