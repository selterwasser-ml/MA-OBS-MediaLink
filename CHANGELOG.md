# Änderungen

## OBS v0.46 BETA – Testbuild

- Neue Einrichtungen starten mit Universum 1. Gespeicherte Universen und IP-Adressen werden beim Update erhalten.
- Multicast verwendet als neue Vorgabe „Automatisch (Windows-Standard)“ statt einer festen persönlichen IP. Gefundene IPv4-Netzwerkkarten lassen sich auswählen; eigene IP-Eingaben bleiben möglich.
- Das Setup und das Windows-ZIP enthalten das lokale MA-Paket mit XML/Lua, beiden GDTF-Fixtures, Anleitungen und einer offline nutzbaren HTML-Startseite. Ein Button im Tab Allgemein öffnet den Ordner.
- Updates ersetzen die mitgelieferten MA-Dateien am gleichen Ort. Der Import in grandMA3 bleibt ein eigener Schritt; MA-Version v0.42 und Fixture-Belegung sind unverändert.
- Der Deinstaller fragt, ob die MediaLink-Einstellungen des angezeigten Windows-Kontos ebenfalls gelöscht werden sollen. Die Auswahl ist standardmäßig ausgeschaltet. Andere OBS-Einstellungen und Plugins bleiben erhalten.
- Finales Projektlogo im Über-Tab, Fenster und Windows-Setup/Deinstallationseintrag; direkter Link zum GitHub-Repository im Über-Tab.
- Erweiterte Tests für frische und bestehende Verbindungswerte, MA-Paket-Updates, Behalten/Löschen der Einstellungen und das Update von der veröffentlichten v0.45.
- Manuelle Installationstests und der Praxistest dieser Version stehen vor der Veröffentlichung noch an.

## Windows-Installer für OBS v0.45 / MA v0.42

- Setup für normale OBS-33-Installationen unter Windows x64 mit Erkennung des OBS-Ordners und Prüfung auf laufendes OBS.
- Aktualisierung einer vorhandenen Installation am gleichen Pluginpfad und Deinstaller; OBS-Konfiguration und MediaLink-Regeln bleiben erhalten.
- Inno-Setup-Compiler mit festgelegter Version und Downloadprüfsumme; Setup und Prüfsumme als zusätzliche GitHub-Buildausgabe.
- Automatisierter Installertest mit künstlichem OBS-Testprogramm. Reale Installation und Laufzeittests bleiben vor einer öffentlichen Beta erforderlich.
- Pluginfunktionen, Versionsnummern, Fixture-Typ-IDs und Kanalbelegung unverändert.

## Build-Infrastruktur für OBS v0.45 / MA v0.42

- GitHub Actions führt die mitgelieferten OBS-/MA-Tests aus und baut anschließend das Plugin für Windows x64 mit Visual Studio 2026.
- OBS 33.0.0 und die OBS-/Qt-Abhängigkeiten sind mit Archivprüfsummen festgelegt; feste lokale SDK-Pfade entfallen im normalen Build.
- Windows-ZIP und separates MA-/Fixture-Paket mit Buildinformationen und SHA256-Prüfsummen.
- Einheitliche Plugin-Ordnerstruktur im CMake-Installationsschritt und Anleitung für Build und ZIP-Installation.
- Pluginfunktionen, Versionsnummern, Fixture-Typ-IDs und Kanalbelegung unverändert.

## OBS v0.45 BETA

- Der frühere manuell platzierbare Crossfade-Quellentyp wird nicht mehr unter „Quelle hinzufügen“ angeboten.
- Der interne Renderer und das Laden vorhandener Altinstanzen bleiben erhalten.
- Die MA-Seite bleibt bei v0.42.

## OBS v0.44 BETA

- Szenenliste im Dock unabhängig von der Diagnose ein- und ausblenden.
- Sichtbarkeit von Diagnose und Szenenliste getrennt speichern und beim Start wiederherstellen.

## OBS v0.43 BETA

- Tonfades von Medienquellen folgen bei unterbrochenen Überblendungen den tatsächlichen Bildanteilen.
- Abhängige Einstellfelder werden passend zur Quellenart und Regel ausgegraut; gespeicherte Werte bleiben erhalten.

## grandMA3 v0.42 BETA

- Normale Schriftgröße für Feldbeschriftungen und passend bemessene Textfelder.
- UTF-8-fähiger Zeilenumbruch und überarbeitete Abstände in den nativen MA-Dialogen.
- Geschützter Import-Einstieg und vorhandene Einrichtung bleiben erhalten.
