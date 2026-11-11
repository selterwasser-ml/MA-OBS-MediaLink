# MA OBS MediaLink v0.46 · BETA

MA OBS MediaLink steuert OBS-Szenen, Video und Audio über grandMA3.

## Neu in v0.46

Bei einer frischen Einrichtung ist **Universum 1** voreingestellt. Für Multicast gilt **Automatisch (Windows-Standard)**; bei mehreren Netzwerkkarten kannst du die passende IPv4-Schnittstelle auswählen oder eine lokale IPv4-Adresse eingeben. Vorhandene gespeicherte Verbindungswerte werden beim Update nicht überschrieben. „Automatisch“ verwendet eine Windows-Standardschnittstelle und bedeutet nicht, dass alle Netzwerkkarten gleichzeitig beigetreten werden.

Im Tab **Allgemein** öffnet **MA-Paket und Anleitungen öffnen** den lokalen Ordner mit dem MA-Plugin v0.43 mit automatischer Cue-Wertvergabe, beiden GDTF-Profilen und der Offline-Startseite `START_HIER.html`. Das Setup aktualisiert die mitgelieferten Dateien; ein Import in grandMA3 erfolgt weiterhin von Hand.

Im Tab **Über** findest du das Projektlogo und **GitHub-Repository öffnen**. Die bisherige Widmung und das Foto bleiben erhalten.

Bei der Deinstallation kannst du zusätzlich deine MediaLink-Einstellungen löschen lassen. Standardmäßig bleiben sie für eine spätere Neuinstallation erhalten. Windows-Konto und Einstellungsordner werden in der Abfrage angezeigt; andere Benutzerprofile, OBS-Szenen und andere Plugins werden nicht bereinigt.

## Alte Crossfade-Quelle

Der frühere Quellentyp für manuell platzierte Crossfade-Quellen wird unter **Quelle hinzufügen** nicht mehr angeboten. MA OBS MediaLink wird über das Dock und **Werkzeuge → MA OBS MediaLink** eingerichtet; eine zusätzliche Quelle in einer Szene ist dafür nicht erforderlich.

Der Quellentyp bleibt intern für die Überblendung und zum Laden vorhandener Szenensammlungen registriert. Bereits platzierte Quellen werden nicht automatisch aus Szenen gelöscht. Die aktuelle MA-Steuerung benötigt diese Altinstanzen nicht.

## Szenenliste im Dock

Die Checkbox **Szenenliste** blendet die Liste der OBS-Szenennummern unabhängig von **Diagnose** ein und aus. Beide Zustände werden getrennt gespeichert und beim nächsten OBS-Start wiederhergestellt. Bei einer älteren Konfiguration ohne diese Einstellung ist die Szenenliste zunächst sichtbar. Die bisherige Diagnose-Auswahl bleibt erhalten.

Die Liste zeigt weiterhin die aktuelle OBS-Szenenreihenfolge und aktualisiert Namen, hinzugefügte bzw. entfernte Szenen. Aus- und Einblenden erhält die vorhandene Scrollposition innerhalb des geöffneten Docks. Die Diagnoseansicht in den Einstellungen zeigt weiterhin die vollständigen Informationen.

## Unterbrochene Video-Tonfades

Der Audiomodus **Mit MA-Fade** folgt jetzt auch bei Medienquellen den tatsächlichen Anteilen der Bildmischung. Wird ein Fade vorzeitig auf eine weitere Szene umgeleitet, bleiben die bisherigen Anteile erhalten. Eine bei 30 % hörbare Quelle beginnt ihren Ausblendvorgang daher bei diesem Anteil und nicht erneut bei voller Lautstärke.

Bei **Abgebrochener Fade → Weiterlaufen** läuft die Wiedergabe weiter; die Lautstärke folgt weiterhin der Bildmischung. Quellen mit Fade-Audioregel werden beim vollständigen Ausblenden stumm, auch wenn ihr Decoder weiterläuft. Quellenlautstärke, native Stummschaltung und Spuren bleiben erhalten.

## Abhängige Einstellfelder

Ausgegraute Felder behalten ihre gespeicherten Werte. Ein Tooltip erklärt den Grund. Zum Bearbeiten muss die MA-Steuerung nicht eingeschaltet sein; das gilt auch für die globale Video-/Audiosteuerung dieser Szenensammlung. Die Aktivierung einer einzelnen Quellenregel schaltet dagegen deren zugehörige Felder frei.

| Videostart | Verfügbare Zusatzfelder |
| --- | --- |
| Bei Fadebeginn / Bei Fadeende | Keine Zeit- oder Prozentfelder |
| Bei X % Fadefortschritt | Startschwelle |
| X Sekunden nach Fadebeginn | Verzögerung |
| X Sekunden vor geplantem Fadeende | Geplante Fadedauer; Startabstand bei bekannter Dauer |
| Nur manuell | Keine Zeit-/Prozentfelder; kein automatischer Start durch direkten Sprung oder Goto auf dieselbe Szene |

Ohne geplante Fadedauer startet die Regel beim tatsächlichen Fadeende. Ein Startabstand größer als die bekannte Fadedauer wird am Feld erklärt und beim Speichern einer aktiven Regel abgewiesen. Inaktive Zeitfelder und deaktivierte Quellenregeln blockieren das Speichern nicht.

Für vorbereitete FFmpeg-Quellen sind die Bildoptionen verfügbar. Ohne Vorbereitung sowie bei VLC zeigt **Am Videoende** die Auswahl **OBS-Quelleneinstellungen verwenden** oder **Wiederholen (Loop)**. Die zuvor gespeicherte Bildoption bleibt bei einem bloßen Wechsel der Vorbereitung erhalten. Blackmagic-Livequellen zeigen ausschließlich die Audiosteuerung. Video- und Audio-Modi bleiben unabhängig wählbar.

Im Tab **Verbindung** ist bei Unicast nur die Unicast Bind-IP bedienbar, bei Multicast nur die Multicast Interface-IP. Beide Adressen bleiben gespeichert; beim Speichern wird die Adresse des gewählten Modus geprüft.

## Blackmagic-Audio

Im Tab **Video und Audio** erscheinen neben Medienquellen jetzt native Blackmagic-Device-Quellen mit eingebettetem Audio. Links die Szene und daneben den mit „Blackmagic · Audio“ gekennzeichneten Eingang auswählen. Für diesen Eingang werden ausschließlich die Aktivierung der Audiosteuerung und der Audiomodus angezeigt.

| Audiomodus | Verhalten |
| --- | --- |
| OBS-Pegel (ohne MA-Fade) | Der normale OBS-Quellenpegel gilt, solange die Szene sichtbar ist. |
| Mit MA-Fade | Ein- und Ausblenden folgen den tatsächlichen Szenenanteilen, auch bei unterbrochenen Fades. |
| Ab Fadeende (100 %) | Eine neu aufgerufene Szene wird erst nach Erreichen des Fadeendes hörbar. Beim Verlassen bleibt der bereits freigegebene Ton bis zum vollständigen Szenenwechsel hörbar. |
| Immer stumm | Der Ton dieser Verwendung wird während aktiver MA-Steuerung unterdrückt. |

Die Regel wird pro Kombination aus Szene und Quelle gespeichert. „Übernehmen“ aktualisiert den Entwurf; „Speichern“ veröffentlicht ihn für die laufende Steuerung. Gespeicherte Blackmagic-Audioänderungen werden ohne erneuten Geräteaufruf übernommen.

Die Blackmagic-Geräteeinstellungen, Videoaufnahme und eingebetteten Audiokanäle werden nicht umkonfiguriert. Die Regel verwendet denselben temporären Audiofilter wie die Mediensteuerung. OBS-Quellenpegel, Stummschaltung, Spurrouting und eigene Quellenfilter gelten weiter. Mehrfach verwendete Eingänge besitzen einen gemeinsamen Audiostream: Die hörbaren Beiträge werden zu einem Pegel zusammengeführt und auf 100 % begrenzt. Bei gleicher Fade-Regel in beiden Szenen bleibt derselbe Eingang ohne Pegelloch hörbar.

Blackmagic-Quellen erhalten keine Video-Transportbuttons und werden nicht durch Medien-Start-/Pause-/Stop-/Loop-Befehle gesteuert. Die Unterstützung gilt für den nativen OBS-Quellentyp „Blackmagic Device“ (decklink-input), nicht für als allgemeines Videoaufnahmegerät angelegte Eingänge anderer Typen.

## Rückgabe an OBS

Beim Ausschalten der MA-Steuerung bzw. Video-/Audiosteuerung werden alle temporären Audiofilter entfernt. Die in v0.37 eingeführte Wiederherstellung der nativen Medienoptionen bleibt erhalten. Auch nach dem Laden einer während aktiver Steuerung gespeicherten Sammlung werden verbliebene MediaLink-Filter auf Blackmagic-Quellen bereinigt.

MediaLink-Regeln bleiben gespeichert und gelten beim nächsten Aktivieren wieder. Die Medienposition wird bei der Rückgabe nicht auf einen früheren Zeitpunkt zurückgespult. Originaloptionen werden seit v0.37 vor dem ersten Eingriff gesichert; historische Änderungen älterer Versionen lassen sich ohne damalige Sicherung nicht rekonstruieren.

## Über

Ein Klick auf Farin und anschließend auf Bela in der Widmung öffnet das unveränderte Foto in einem separaten Fenster. Es lässt sich über „Schließen“, Escape oder das Fensterkreuz schließen. Das vollständige Foto wird proportional eingepasst und ist als Qt-Ressource direkt in der DLL enthalten. Eine zusätzliche Bilddatei im Installationsverzeichnis ist nicht erforderlich.

## Installation

Für die normale Installation das [Windows-Setup](../docs/WINDOWS_INSTALLER.md) aus dem [automatischen Windows-Build](../docs/BUILD.md) verwenden. Alternativ gilt die [ZIP-Installationsanleitung](../docs/WINDOWS_ZIP.md). Zum lokalen Bauen werden Visual Studio 2026 mit C++-Werkzeugen und CMake ab 3.28 benötigt. PowerShell im entpackten OBS-Ordner öffnen:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Build_OBS33_Release.ps1"
```

Nach erfolgreichem Build das vollständige Release-Paket nach ZIP-Anleitung installieren oder das Setup bauen und ausführen. Das ältere Entwicklerskript `Install_Release.ps1` tauscht nur DLL und Shader aus; es ersetzt keine vollständige v0.46-Installation mit lokalem MA-Paket und Logo. Interne DLL-, Quellen-, Dock- und Konfigurationskennungen bleiben kompatibel. Das Paket enthält die grandMA3-Bridge v0.43 mit automatischer Cue-Wertvergabe. Die vorhandene Bridge v0.42 bleibt mit dem unveränderten OBS-Protokoll kompatibel; für die neue Bedienung muss v0.43 in grandMA3 importiert werden.

## Prüfstand

Die mitgelieferten Tests prüfen ausgewählte Teile der Steuerung. Sie ersetzen keinen Test des vollständigen Plugins in OBS und auf grandMA3. Der Windows-Build lädt die in `dependencies.json` festgelegten OBS-33- und Qt6-Abhängigkeiten selbstständig und prüft die Archivprüfsummen. Das lokale Installationsskript setzt eine bestehende Installation unter `C:\ProgramData\obs-studio\plugins\obs-sacn-crossfade` voraus und sichert diese vor dem Austausch. Für eine Erstinstallation die [ZIP-Installationsanleitung](../docs/WINDOWS_ZIP.md) verwenden. Der automatische Build führt das Installationsskript nicht aus.

Copyright © 2026 Oliver Cassel

Für Farin und Bela.
