# Abhängigkeiten und Inhalte

| Bestandteil | Verwendung | Herkunft und Lizenzhinweis |
| --- | --- | --- |
| OBS Studio / libobs / Frontend-API | Externe Laufzeitbibliotheken und Build-SDK | [OBS Studio 33.0.0](https://github.com/obsproject/obs-studio/tree/33.0.0), [GPL Version 2 oder später](https://github.com/obsproject/obs-studio/blob/33.0.0/COPYING) |
| Qt 6 Core / Widgets | Dynamisch gelinkte Benutzeroberfläche; wird von OBS bereitgestellt | [Qt-Lizenzinformationen](https://doc.qt.io/qt-6/licensing.html); Open-Source-Lizenzbedingungen des verwendeten Moduls beachten |
| OBS- und Qt-Buildabhängigkeiten | Heruntergeladene SDK-/Builddateien | [OBS deps 2026-10-23](https://github.com/obsproject/obs-deps/releases/tag/2026-10-23); Versionen und Archivprüfsummen in `OBS/dependencies.json` |
| Foto Farin und Bela | Unverändert als Qt-Ressource in die Plugin-DLL eingebettet | © 2026 Oliver Cassel, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| MediaLink-Projektlogo | Vom Projektinhaber bereitgestelltes finales PNG; daraus abgeleitete Windows-Icongrößen | Oliver Cassel; Verwendung als Projektlogo. Die bereitgestellte PNG-Datei bleibt unverändert erhalten. |
| Inno Setup | Compiler und Setup-/Deinstaller-Laufzeit | [Inno Setup 6.7.4](https://github.com/jrsoftware/issrc/releases/tag/is-6_7_4), [Lizenz](https://github.com/jrsoftware/issrc/blob/is-6_7_4/license.txt); Compilerprüfung in `installer/compiler.json` |

Die erzeugten ZIPs und das Setup enthalten keine Kopien der OBS- oder Qt-Laufzeit-DLLs und keine SDK-Archive. Die Abhängigkeiten werden nicht als Bestandteil von MediaLink neu lizenziert. Das Setup enthält die vom Inno-Setup-Compiler erzeugte Installations- und Deinstallationslaufzeit.

MediaLink-Code und GDTF-Profile stehen unter GPL-2.0-or-later; siehe `LICENSE`. grandMA3 ist ein Produkt von MA Lighting und wird nicht mitgeliefert. Dieses Projekt ist keine offizielle Veröffentlichung von OBS Project oder MA Lighting.
