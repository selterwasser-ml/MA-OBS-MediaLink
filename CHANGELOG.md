# Änderungen

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
