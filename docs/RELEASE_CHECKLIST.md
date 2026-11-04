# Checkliste für die erste öffentliche Beta

## Repository und Lizenzen

- [x] Privates Repository im persönlichen Konto `selterwasser-ml` anlegen.
- [x] GNU GPL Version 2 oder später für den Programmcode festlegen und Lizenztext aufnehmen.
- [x] CC BY 4.0 für das selbst aufgenommene Foto festlegen und Bildnachweis aufnehmen.
- [x] README und projektspezifische .gitignore aufnehmen.
- [ ] Anmerkungen zur Repository-Struktur einarbeiten.
- [ ] Verständliche Namen für Fade- und Steuer-Fixture festlegen; vorhandene Typ-IDs und Kanalbelegung erhalten.
- [ ] Aktuellen OBS-Code v0.45, MA-Code v0.42 und beide vorhandenen Fixtureprofile importieren.
- [ ] Lizenzkennungen im importierten Code sowie Drittanbieterhinweise prüfen und ergänzen.
- [ ] Bildnachweis und Lizenzlink im Foto-Fenster der nächsten OBS-Version ergänzen.
- [ ] Installation, Kurzstart, Patch und Cue-Notes für neue Nutzer dokumentieren.
- [ ] Genaue getestete OBS-, grandMA3- und Windows-Versionen erfassen.

## Build und Installer

- [ ] Reproduzierbaren Windows-x64-Build mit festgelegten OBS-/Qt-Abhängigkeiten einrichten.
- [ ] GitHub Actions für Prüfungen und Windows-Build einrichten.
- [ ] Installer mit stabilem AppId sowie Update- und Deinstallationsablauf erstellen.
- [ ] Bestehende Plugin-Kopien unter Erhalt persönlicher Einstellungen migrieren.
- [ ] Unterstützung portabler OBS-Installationen festlegen.
- [ ] Erforderliche Laufzeitbibliotheken und Ressourcen prüfen.

## Test und Veröffentlichung

- [ ] Erstinstallation, Update, Deinstallation und Neuinstallation auf sauberem Windows testen.
- [ ] Szenenwechsel, Bild-/Tonfades, Fade-Abbruch, Quellenrückgabe, Neustart und Shutdown prüfen.
- [ ] Blackmagic-Audio mit realer Hardware prüfen.
- [ ] Fertige Downloads, Versionshinweise und Prüfsummen als Beta-Release-Entwurf bereitstellen.
- [ ] Digitale Signatur entscheiden.
- [ ] Getesteten Stand öffentlich veröffentlichen.
