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
| Installer | Windows x64 / normale OBS-33-Installation; auch ohne Entwicklerwerkzeuge getestet |

Dieses Repository enthält Quellcode und einen [automatischen Windows-Build](docs/BUILD.md). Nach erfolgreichen Tests stellt GitHub Windows-Setup, Windows-ZIP und das passende MA-/Fixture-Paket bereit. Der Projektbetreiber hat **OBS Studio 33.0.1** mit **grandMA3 2.5.0.3** positiv getestet. Die saubere Installation auf einem zweiten Rechner ohne Entwicklerwerkzeuge wurde ebenfalls bestätigt. Diese Rückmeldungen beziehen sich auf den getesteten Aufbau; andere Versionen und Hardwarekombinationen sind damit nicht pauschal geprüft.

Die [Beta-Versionshinweise](releases/v0.45.0-beta.1.md) nennen Download-Dateien, Teststand und bekannte Einschränkungen. Der [Release-Ablauf](docs/BETA_RELEASE.md) übernimmt den festgelegten getesteten Build in einen unveröffentlichten Entwurf. [Spätere Erweiterungen](docs/NEXT_STEPS.md) bleiben separat vorgemerkt.

## Einstieg

1. [Windows-Paket über GitHub bauen](docs/BUILD.md) und [Setup ausführen](docs/WINDOWS_INSTALLER.md), alternativ [manuell installieren](docs/WINDOWS_ZIP.md) oder [lokal aus dem Quellcode bauen](docs/BUILD.md#lokal-bauen). MediaLink über das Dock oder **Werkzeuge → MA OBS MediaLink** einrichten. Eine zusätzliche Crossfade-Quelle in einer Szene ist nicht erforderlich.
2. [Fixtureprofile importieren und patchen](fixtures/README.md). Beide Fixtures verwenden zusammen zehn aufeinanderfolgende DMX-Adressen im selben Universum.
3. [grandMA3-Plugin importieren und einrichten](grandMA3/README_START_HIER.md). Die XML- und Lua-Datei aus grandMA3/MA3/ gemeinsam verwenden. Lesesequenz, Fixture-IDs, Steuersequenz und Presetbereich werden im Plugin festgelegt.
4. Fade-Preset ausdrücklich in Cue Part 0 speichern. Die Cue-Note `OBS:1` ruft die erste Szene in der OBS-Szenenliste auf, `OBS:6` die sechste. Großbuchstaben, Doppelpunkt, Nummer; ohne Leerzeichen oder führende Nullen.
5. Universum und Startadresse auf beiden Seiten abgleichen und die Verbindung über die Diagnose prüfen.

Eine Änderung der Reihenfolge in OBS verändert die Szenennummern. Die Lesesequenz ist unabhängig von der aktuell ausgewählten grandMA3-Sequenz. Die Bridge läuft auf einer vorgesehenen MA-Station.

## Verzeichnisse

| Verzeichnis | Inhalt |
| --- | --- |
| OBS/ | C++-/Qt-Plugin, Ressourcen, Windows-Buildskripte, festgelegte Abhängigkeiten und Tests |
| grandMA3/ | Importdateien, wartbarer Lua-Kern, Erzeugungswerkzeug und Tests |
| fixtures/ | GDTF-Profile und Patchanleitung |
| installer/ | Windows-Setup, festgelegter Compiler und Installertest |
| docs/ | Automatischer Build und Installation des Windows-ZIPs |
| tools/ | Gemeinsamer Testaufruf und Erzeugung der Downloadpakete |

[Änderungen](CHANGELOG.md) · [Lizenz](LICENSE) · [Drittanbieter und Inhalte](THIRD_PARTY.md)

## Lizenz

Der Programmcode und die Fixtureprofile stehen unter der GNU GPL Version 2 oder später (GPL-2.0-or-later). Das selbst aufgenommene Foto [Farin und Bela](OBS/data/cats_farin_bela.png) stammt von Oliver Cassel und steht unter [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/deed.de). Bei Weiterverwendung des Fotos bitte den Namen und die Lizenz nennen. OBS Studio und grandMA3 sind Produkte ihrer jeweiligen Hersteller; dieses Projekt ist keine offizielle Veröffentlichung der Hersteller.
