# Abhängigkeiten und Inhalte

| Bestandteil | Verwendung | Herkunft und Lizenzhinweis |
| --- | --- | --- |
| OBS Studio / libobs / Frontend-API | Externe Laufzeitbibliotheken und Build-SDK | [OBS Studio 33.0.0](https://github.com/obsproject/obs-studio/tree/33.0.0), [GPL Version 2 oder später](https://github.com/obsproject/obs-studio/blob/33.0.0/COPYING) |
| Qt 6 Core / Widgets | Dynamisch gelinkte Benutzeroberfläche; wird von OBS bereitgestellt | [Qt-Lizenzinformationen](https://doc.qt.io/qt-6/licensing.html); Open-Source-Lizenzbedingungen des verwendeten Moduls beachten |
| OBS- und Qt-Buildabhängigkeiten | Heruntergeladene SDK-/Builddateien | [OBS deps 2026-10-23](https://github.com/obsproject/obs-deps/releases/tag/2026-10-23); Versionen und Archivprüfsummen in `OBS/dependencies.json` |
| Foto Farin und Bela | Unverändert als Qt-Ressource in die Plugin-DLL eingebettet | © 2026 Oliver Cassel, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |

Die erzeugten ZIPs enthalten keine Kopien der OBS- oder Qt-Laufzeit-DLLs und keine SDK-Archive. Die Abhängigkeiten werden nicht als Bestandteil von MediaLink neu lizenziert. Falls der spätere Installer weitere Laufzeitdateien mitliefert, müssen deren Lizenztexte und Weitergabebedingungen zusätzlich berücksichtigt werden.

MediaLink-Code und GDTF-Profile stehen unter GPL-2.0-or-later; siehe `LICENSE`. grandMA3 ist ein Produkt von MA Lighting und wird nicht mitgeliefert. Dieses Projekt ist keine offizielle Veröffentlichung von OBS Project oder MA Lighting.
