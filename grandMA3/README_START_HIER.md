# MA OBS MediaLink – grandMA3 v0.43 BETA

Testversion vom 11. November 2026. Grundlage: GitHub main
`3a49dc48d842e37e707bec3a287991257389ca42` (MA v0.42, OBS v0.46).
Der OBS-Teil benötigt für diese Änderung keinen neuen Build; das bestehende
3+7-Kanal-Protokoll und die beiden Fixtureprofile bleiben erhalten.

## Import und Einrichtung

1. Die neue Version zunächst in einer Showkopie prüfen.
2. `MA3/MA_OBS_MediaLink.xml` und die gleichnamige Lua-Datei gemeinsam
   in den MA-Plugin-Importordner legen. XML importieren; beide Komponenten
   (Bedienung und Autostart) gehören zu demselben Plugin.
3. Unter Einrichtung Datenpool, auszulesende Sequenz, Fade-Fixture,
   Steuer-Fixture, Steuersequenz und Startverhalten festlegen.
   Bestehende Einstellungen werden übernommen; frühere Vergabeeinstellungen entfallen.
4. Fixtures wie bisher patchen, siehe `fixtures/README.md`. Diagnose prüfen.
5. An den gewünschten Cues eine Note wie `OBS:1` oder `OBS:12` eintragen.
   Die Zahl bezeichnet weiterhin die Position in der OBS-Szenenliste.
6. Bridge starten. Beim ersten Timerlauf und danach ungefähr einmal pro
   Sekunde werden direkte Fade-Werte in Cue Part 0 abgeglichen.

## Automatische Werte

Eine neue Sequenz erhält zunächst 10, 20, 30 usw. Zwischen vorhandenen
Werten werden freie Ganzzahlen genutzt: Zwischen 10 und 20 zuerst 15.
Mehrere gleichzeitig eingefügte Cues werden in ihrer Cue-Reihenfolge verteilt.
Bestehende Werte bleiben erhalten, solange sie eindeutig und aufsteigend sind.
Beim Anhängen wird zunächst mit Abstand 10 weitergearbeitet.

Ist die benötigte Lücke voll, reicht der Bereich am Ende nicht mehr oder
sind durch Kopieren/Verschieben doppelte bzw. vertauschte Werte entstanden,
wird die gesamte Lesesequenz neu verteilt. Bis 102 Cues sind Abstände von 10
möglich; danach verwendet die Neuverteilung kleinere Abstände. Es stehen
weiterhin genau 1024 logische Positionen (0–1023) mit der bisherigen
24-Bit-Skalierung zur Verfügung. Bei 1024 Cues werden alle Positionen benutzt.
Mehr als 1024 gültige OBS-Cues werden vor dem Schreiben abgewiesen.
Die logischen Zahlen sind keine Prozentwerte und keine Fadezeiten.

Wird eine OBS-Note entfernt, wird beim nächsten Abgleich der ausdrücklich
gespeicherte VideoCrossfade-Wert des eingerichteten Fade-Fixtures aus Part 0
entfernt. Die Nummer ist anschließend wieder frei. Ohne ausdrücklich
gespeicherten Wert gilt das normale MA-Tracking weiter; das Entfernen
speichert also keinen Null- oder Release-Wert. Eine ungültige OBS-Note wird
als Diagnoseproblem behandelt und ist keine Löschanforderung.

Das Fade-Fixture ist in dieser Lesesequenz für MediaLink reserviert. Seine
Werte werden vom Plugin verwaltet, auch wenn sie zuvor manuell angelegt wurden.
Die direkte Zuordnung in den Cues bleibt beim Neustart erhalten.

## IndivFades und vorhandene Shows

Das Fade-Fixture bleibt normal auswählbar. IndivFades und individuelle Delays
können in den Cues bearbeitet werden. Neue Werte werden gezielt mit Merge
gespeichert; das Plugin aktiviert nur den absoluten Wert, keine Zeitschicht.
Vorhandene Fade-/Delay-Felder werden nach dem Schreiben auf Erhalt geprüft.
Cue-Namen, Notes, allgemeine Cue-Zeiten und andere Fixtures werden nicht
bearbeitet. Das Fade-Fixture gehört in Part 0; Daten dieses Fixtures in
weiteren Parts führen zu einer Meldung statt einer stillen Änderung.

Solange das Fade-Fixture Programmer-Daten enthält, wartet ein erforderlicher
Abgleich. Das Plugin leert keinen fremden Programmer und stellt die Auswahl
über eine temporäre Gruppe wieder her. Für einen automatischen Abgleich die
Bearbeitung des Fade-Fixtures im Programmer abschließen und freigeben.

Alte Referenzen werden in direkte Werte umgewandelt. Vorhandene Poolobjekte
werden dabei nicht gelöscht, da sie auch außerhalb der Lesesequenz verwendet
sein können. Der neue Betrieb erzeugt, benötigt und verwaltet keine solchen
Poolobjekte mehr; die frühere Bedienseite und ihre Einstellungen entfallen.

Quellwerte werden vor dem Versand der passenden Steuerdaten abgeglichen.
Bei einem Schreib- oder Rücklesefehler stoppt die Bridge; es gibt keine
automatische Wiederholung des fehlgeschlagenen Schreibvorgangs. Bereits
ausgeführte Änderungen werden bei einem späteren Fehler nicht automatisch
zurückgerollt. Ursache prüfen, dann die Bridge gezielt neu starten.
Eine Neuverteilung ändert nicht durch sich selbst die bereits aktive Szene.

## Teststand und Praxistest

Automatische Tests unter Lua 5.4 bestanden: Vergabe, Zwischenwerte, volle
Lücken, Neuverteilung, Löschen/Wiederverwenden, Kopieren/Verschieben,
Neustart, Umstieg, IndivFades im Testadapter, andere Fixture-Daten,
1024 Cues und Überlauf, 600 Bearbeitungszyklen, Schreib-/Rücklesefehler,
Sender-Sperre, Steuerdaten, Startverhalten und Oberfläche. Die importierbare
Lua-Datei enthält nachweislich genau den aktuellen Kern.

Der Anwender hat v0.43 am 11. November 2026 ausprobiert und die Funktion
mit „funktioniert top“ bestätigt. Der genaue Testumfang und die verwendeten
Versionen wurden für diesen Durchlauf nicht gesondert dokumentiert.
Der Adapter bildet weder den MA-Store-/Tracking-Mechanismus noch physische
DMX-Ausgabe oder zeitgleiche Änderungen anderer Stationen vollständig ab.
Vor allem Merge mit bestehenden IndivFades und das Entfernen in einer
getrackten Sequenz müssen auf der eingesetzten MA-Version geprüft werden.

Für weitere gezielte Praxistests: OBS-Cues mit verschiedenen IndivFades anlegen,
Bridge starten, Cue zwischen 10 und 20 einfügen, Note entfernen und wieder
setzen, eine Lücke ausschöpfen und anschließend neu starten. Werte,
IndivFades, Tracking und Szenenaufrufe dabei kontrollieren. Änderungen und
Neuverteilungen zunächst außerhalb laufender Übergänge prüfen.

## Quellcode und Tests

Kern: `source/medialink_core.lua`.
Erzeugen: `python grandMA3/tools/build_plugin.py`.
Konsistenz: `python grandMA3/tools/build_plugin.py --check`.
Regressionen: im Ordner `grandMA3` mit Lua 5.4 oder neuer
`lua tests/test_medialink.lua` ausführen.

Copyright © 2026 Oliver Cassel. Für Farin und Bela.
GPL-2.0-or-later, siehe LICENSE.
