# MA OBS MediaLink · BETA

MA OBS MediaLink verbindet grandMA3 mit OBS Studio. Szenenwechsel, Bildüberblendung sowie Video- und Audiosteuerung folgen den OBS-Cues einer festgelegten grandMA3-Sequenz über sACN.

Copyright © 2026 Oliver Cassel

*Für Farin und Bela.*

## Stand

| Komponente | Stand |
| --- | --- |
| OBS-Plugin | v0.45 BETA, Quellpaket für Windows x64 / OBS 33 |
| grandMA3-Plugin | v0.42 BETA |
| Fixtureprofile | MA OBS MediaLink Fade (3 Kanäle) und MA OBS MediaLink Steuerung (7 Kanäle) |
| Installer | In Vorbereitung |

Dieses Paket enthält Quellcode und Buildskripte. Eine vorkompilierte DLL oder ein ausführbarer Installer ist noch nicht enthalten. Der vollständige Windows-Build der OBS-Version v0.45 steht aus. Die MA-Oberfläche v0.42 wurde auf WebRemote und Hardware positiv getestet; die genaue grandMA3-Softwareversion wird für die öffentliche Kompatibilitätsliste noch erfasst.

## Einstieg

1. [OBS bauen und installieren](OBS/README.md). MediaLink über das Dock oder **Werkzeuge → MA OBS MediaLink** einrichten. Eine zusätzliche Crossfade-Quelle in einer Szene ist nicht erforderlich.
2. [Fixtureprofile importieren und patchen](fixtures/README.md). Beide Fixtures verwenden zusammen zehn aufeinanderfolgende DMX-Adressen im selben Universum.
3. [grandMA3-Plugin importieren und einrichten](grandMA3/README_START_HIER.md). Die XML- und Lua-Datei aus grandMA3/MA3/ gemeinsam verwenden. Lesesequenz, Fixture-IDs, Steuersequenz und Presetbereich werden im Plugin festgelegt.
4. Fade-Preset ausdrücklich in Cue Part 0 speichern. Die Cue-Note `OBS:1` ruft die erste Szene in der OBS-Szenenliste auf, `OBS:6` die sechste. Großbuchstaben, Doppelpunkt, Nummer; ohne Leerzeichen oder führende Nullen.
5. Universum und Startadresse auf beiden Seiten abgleichen und die Verbindung über die Diagnose prüfen.

Eine Änderung der Reihenfolge in OBS verändert die Szenennummern. Die Lesesequenz ist unabhängig von der aktuell ausgewählten grandMA3-Sequenz. Die Bridge läuft auf einer vorgesehenen MA-Station.

## Verzeichnisse

| Verzeichnis | Inhalt |
| --- | --- |
| OBS/ | C++-/Qt-Plugin, Ressourcen, lokale Windows-Buildskripte und Tests |
| grandMA3/ | Importdateien, wartbarer Lua-Kern, Erzeugungswerkzeug und Tests |
| fixtures/ | GDTF-Profile und Patchanleitung |

[Änderungen](CHANGELOG.md) · [Lizenz](LICENSE)

## Lizenz

Der Programmcode und die Fixtureprofile stehen unter der GNU GPL Version 2 oder später (GPL-2.0-or-later). Das selbst aufgenommene Foto [Farin und Bela](OBS/data/cats_farin_bela.png) stammt von Oliver Cassel und steht unter [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/deed.de). Bei Weiterverwendung des Fotos bitte den Namen und die Lizenz nennen. OBS Studio und grandMA3 sind Produkte ihrer jeweiligen Hersteller; dieses Projekt ist keine offizielle Veröffentlichung der Hersteller.
