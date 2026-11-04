# MA OBS MediaLink v0.45 BETA · Windows x64

Dieses Paket enthält die fertige DLL für OBS Studio 33 (Windows x64) und den benötigten Shader. Es ist ein ZIP-Paket; der Installer ist noch in Vorbereitung. Der Build verwendet das SDK von OBS 33.0.0 und Qt 6.11.1. Ein erfolgreicher Build ersetzt keinen Test im eigenen OBS-Aufbau.

## Manuell installieren oder aktualisieren

1. OBS vollständig schließen.
2. Bei einem Update den vorhandenen Ordner `C:\ProgramData\obs-studio\plugins\obs-sacn-crossfade` an einem anderen Ort sichern.
3. Den hier enthaltenen Ordner **obs-sacn-crossfade** nach `C:\ProgramData\obs-studio\plugins\` kopieren. Für diesen Ordner können Administratorrechte erforderlich sein. Der resultierende Pfad zur DLL lautet `C:\ProgramData\obs-studio\plugins\obs-sacn-crossfade\bin\64bit\obs-sacn-crossfade.dll`.
4. OBS starten und **Werkzeuge → MA OBS MediaLink** öffnen. Keine zusätzliche Crossfade-Quelle in einer Szene anlegen.

Keine zweite Kopie derselben Plugin-DLL in einem anderen OBS-Pluginordner installieren. Bei einer bisherigen Installation an einem anderen Ort zunächst deren Pluginpfad prüfen. Das ZIP-Verfahren ist für eine normale OBS-Installation gedacht; portable Installationen werden hier noch nicht abgedeckt.

Das Kopieren ersetzt nur Plugin-Dateien. MediaLink-Regeln und OBS-Einstellungen bleiben in der vorhandenen OBS-Konfiguration. Diese Einstellungen vor dem ersten Test zusätzlich sichern. Das Foto von Farin und Bela ist bereits in der DLL eingebettet.

Die ausführliche OBS-Anleitung liegt unter `OBS/README.md`. Das passende MA-Paket enthält die Importdateien für grandMA3 v0.42 und beide Fixtureprofile.

## Herkunft und Lizenz

`BUILD-INFO.json` nennt die Versionen, den Quellstand (bei GitHub-Builds die Commit-ID), die Abhängigkeiten und die Prüfsumme der DLL. `SHA256SUMS.txt` wird neben den ZIP-Paketen bereitgestellt. Lokal lässt sich eine ZIP-Prüfsumme mit `Get-FileHash -Algorithm SHA256 <Dateiname>` vergleichen.

Copyright © 2026 Oliver Cassel. Programmcode und Fixtures: GPL Version 2 oder später, siehe `LICENSE`. Foto Farin und Bela: Oliver Cassel, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Weitere Hinweise: `THIRD_PARTY.md`.

Quellcode: [selterwasser-ml/MA-OBS-MediaLink](https://github.com/selterwasser-ml/MA-OBS-MediaLink). Vor einer öffentlichen Weitergabe der Binärpakete muss der zugehörige Quellstand ebenfalls zugänglich sein.
