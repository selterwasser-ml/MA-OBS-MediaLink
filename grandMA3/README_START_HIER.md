# MA OBS MediaLink – MA-Seite v0.42 BETA

Szenen, Video und Audio aus grandMA3 steuern.

Ein Plugin mit den Bereichen Betrieb, Einrichtung, Presets, Steuersequenz, Diagnose und Über. Diese Version gehört zum gemeinsamen Paket mit OBS v0.45 und den beiden mitgelieferten Fixtureprofilen.

Copyright © 2026 Oliver Cassel

*Für Farin und Bela.*

Diese Beta-Version befindet sich in Entwicklung.

## Darstellung in v0.42

Die Feldbeschriftungen verwenden jetzt ausdrücklich die normale Schriftgröße. Textfelder, Auswahlbuttons und Befehlsbuttons werden anhand ihrer Zeilenzahl bemessen und erhalten mehr Platz oberhalb und unterhalb des Texts. Lange Beschriftungen werden umgebrochen. Der Zeilenumbruch zählt UTF-8-Zeichen und trennt Umlaute nicht in einzelne Bytes.

Die geschützte Initialisierung und die Compilerkorrektur aus v0.41 bleiben erhalten. UI-Einstellungen und gespeicherte Einrichtung werden übernommen.

## Import und erster Start

1. Die bisherige Bridge stoppen. Vorhandene Presets und Steuersequenzen erhalten.
2. `MA3/MA_OBS_MediaLink.xml` und `MA3/MA_OBS_MediaLink.lua` gemeinsam in den Plugin-Ordner der MA kopieren und die XML importieren. Für den ersten Test einen freien Plugin-Pooleintrag verwenden. Es bleibt ein Pool-Plugin mit zwei internen Komponenten. Alte und neue Version nicht parallel aufrufen.
3. Das Plugin öffnen und unter Einrichtung die auszulesende Sequenz, beide Fixtures und die Steuersequenz prüfen. Die bisherigen gespeicherten Einstellungen werden übernommen.
4. Unter Presets den All-Pool, den Positionsbereich und die Erzeugungsoptionen einstellen. Vorschau zeigt den Plan; erst Jetzt erzeugen erstellt Presets. Speichern allein erzeugt keine Presets.
5. Unter Prüfen / Diagnose den Patch und die OBS-Cues kontrollieren. Anschließend die Bridge starten. Das Bedienfenster darf geschlossen werden; die Bridge läuft weiter.
6. Einrichtung und gewünschten Start-/Stop-Zustand mit der Show speichern.

Ein bereits laufender Betrieb wird beim erneuten Öffnen des Plugins erhalten. Für Änderungen an Einrichtung, Presets oder Steuersequenz zuerst im Bedienfenster stoppen.

## Bedienoberfläche

Betrieb, Einrichtung und Presets verwenden native MA-Dialogelemente mit linksbündigen Texten, klaren Absätzen und ausreichend breiten Eingabefeldern. Der Betriebshinweis zum Stoppen wird hervorgehoben. Bei schmaleren Anzeigen werden die Betriebsbuttons auf zwei Reihen verteilt. Auf sehr kleinen Anzeigen wird bei Bedarf der kompakte MA-Standarddialog verwendet.

Die Diagnose enthält getrennte Abschnitte für Status, Einrichtung, Sender, Fehler und Cue-Einrichtung. Technische Detaildaten stehen im System Monitor.

## Einrichtung

| Einstellung | Bedeutung |
|---|---|
| Datenpool | Datenpool mit den verwendeten Sequenzen und Presets; während des Betriebs aktiv lassen |
| Auszulesende Sequenz | Feste Sequenz mit den OBS-Cues; unabhängig von der aktuell ausgewählten Sequenz |
| Fade-Fixture | MA OBS MediaLink Fade mit VideoCrossfade in 24 Bit |
| Steuer-Fixture | MA OBS MediaLink Steuerung für Szene, Endwert und Ereigniskennung |
| Steuersequenz | Für MediaLink verwendete Sequenz; muss sich von der Lesesequenz unterscheiden |
| Beim Öffnen des Plugins | Verhalten beim Aufruf des Plugin-Pooleintrags |

Speichern prüft die Eingaben. Fixtures, Patch und Cue-Zuordnungen lassen sich anschließend unter Prüfen / Diagnose kontrollieren. Der Patch wird dabei nicht verändert.

Beide Fixtures werden passend aufeinanderfolgend im selben Universum gepatcht:

| Offset zur OBS-Startadresse | Inhalt | Breite |
|---|---|---|
| +0 bis +2 | VideoCrossfade | 24 Bit |
| +3 bis +4 | VideoSceneNumber | 16 Bit |
| +5 bis +7 | Dimmer als Endwert | 24 Bit |
| +8 bis +9 | VideoTransitionToken | 16 Bit |

Universum und Startadresse in der Diagnose müssen den OBS-Einstellungen entsprechen. Für den Zehn-Kanal-Block sind Startadressen von 1 bis 503 möglich.

## Verhalten beim Öffnen

| Auswahl | Verhalten bei gestoppter Bridge |
|---|---|
| Bridge starten | Die gespeicherte Einrichtung prüfen und die Bridge starten |
| Nur Bedienfenster öffnen | Das Fenster öffnen; die Bridge bleibt zunächst aus |
| Letzten Zustand wiederherstellen | Den mit der Show gespeicherten Start-/Stop-Zustand anwenden |

Läuft die Bridge bereits, bleibt sie bei allen drei Einstellungen aktiv. Das erneute Öffnen setzt weder die aktuelle Cue noch die Zuordnung zurück. Stoppen wirkt innerhalb des geöffneten Bedienfensters weiter; die gewählte Startoption wird erst beim nächsten Öffnen erneut angewandt. Ein fehlgeschlagener automatischer Start lässt das Bedienfenster mit der Fehlermeldung erreichbar.

Die Option bezieht sich auf das Öffnen des Plugins. Sie startet das Plugin auf einer Hardwarekonsole nicht selbst beim Einschalten. Die Show speichert Einrichtung und gewünschten Zustand; das Fortbestehen eines laufenden Lua-Timers beim Laden einer Show ist nicht garantiert. Falls die Bridge nach dem Laden nicht weiterläuft, das Plugin einmal öffnen.

### Optionaler onPC-Aufruf

Die zweite interne Komponente kann die Startoption ohne Bedienfenster anwenden:

```text
Plugin <Nummer>.2
```

MA dokumentiert für onPC den Programmstartparameter:

```text
HOSTTYPE=onPC RUNPLUGIN=\"MA_OBS_MediaLink.xml\"-2
```

Die Plugin-Datei muss in der passenden Installation vorhanden sein. Dieser Aufruf versucht bis zu 20 Sekunden auf die gespeicherte Einrichtung zu warten. Der vollständige Ablauf mit der jeweiligen Show ist auf der eingesetzten onPC-Version zu prüfen. Das Paket verändert keine Programmverknüpfung.

## Steuersequenz

Die Steuersequenz heißt **MediaLink – Nicht löschen**. Beim ersten Start übernimmt das Plugin die bisherige, eindeutig zugeordnete MediaLink-Steuersequenz unter diesem Namen. Die vorhandenen Cues werden dafür weder neu erstellt noch gelöscht oder umnummeriert.

Die Zuordnung steht zusätzlich als Kennungszeile in der Sequenznotiz. Eigene Notizen bleiben erhalten. Die MediaLink-Kennungszeile beibehalten, damit die Sequenz unabhängig vom sichtbaren Namen wiedererkannt wird.

Eine anderweitig belegte Sequenz wird nur über Steuersequenz nach Prüfung und ausdrücklicher Bestätigung übernommen. Zusätzliche Fixtures, weitere Cue-Parts oder Cue-Commands werden abgewiesen. Eine freie Nummer wird beim ersten gültigen Abgleich angelegt.

## Presets

| Einstellung | Bedeutung |
|---|---|
| All-Presetpool | All 1 bis All 5 |
| Erste Presetposition | Beginn des Bereichs mit 1024 festen Positionen |
| Anzahl zusätzlich | Zahl neu zu erzeugender Presets, 1 bis 1024 |
| Vorgehen | Hinten ergänzen oder vorhandene Lücken ergänzen |
| Abstand beim Anhängen | Abstand der neuen Positionen beim Anhängen; beim Lückenfüllen ohne Wirkung und im neuen Dialog deaktiviert |

Vorschau zeigt die tatsächlich möglichen Positionen. Erst Jetzt erzeugen erstellt die angezeigten Presets. Vorhandene und fremde Presets werden nicht überschrieben. Eine Änderung der Startposition verschiebt keine vorhandenen Presets.

Beispiel: Startposition 101, Abstand 10, Anzahl 10 erzeugt zunächst 101, 111 … 191. Ein weiterer Aufruf ergänzt 201, 211 … 291. Lücken ergänzen füllt dagegen die freien Positionen innerhalb dieses bestehenden Bereichs, unabhängig vom Anhängeabstand. Dieser Modus erweitert den Bereich nicht.

Die Zuordnung der Fade-Werte bleibt fest: Position i von 0 bis 1023 entspricht `round(i × 16777215 / 1023)`. Vorhandene Werte werden beim Ergänzen nicht verändert. Neue Presets heißen OBS ML 0000 bis OBS ML 1023; passende ältere OBS-Presets bleiben nutzbar.

## OBS-Cues programmieren

1. Das Fade-Preset für das Fade-Fixture ausdrücklich in Cue Part 0 der Lesesequenz speichern.
2. Als Cue-Note beispielsweise `OBS:1` oder `OBS:6` eintragen. Die Nummer bezeichnet die Position in der OBS-Szenenliste. Einheitliche Schreibweise ohne Leerzeichen und führende Nullen.

`OBS:1` bezeichnet die erste, `OBS:6` die sechste und `OBS:12` die zwölfte Szene. Die Szenennummer ist unabhängig von der 24-Bit-Fadeposition. Zwei Cues können dieselbe Szenennummer und unterschiedliche Fade-Presets verwenden. Änderungen der OBS-Szenenreihenfolge ändern die Zuordnung.

Bei laufender Bridge werden Zuordnungen vorbereitet und Änderungen schrittweise ergänzt. Vorhandene passende Steuer-Cues werden wiederverwendet. Geänderte Zuordnungen erhalten neue Steuer-Cues; alte Einträge bleiben erhalten. Die Steuersequenz kann deshalb bei wiederholten Änderungen wachsen.

Ein Wechsel der aktiven Cue löst die Ausgabe aus. Eine Änderung an der gerade aktiven Cue gilt beim nächsten Eintritt. Ein erneutes Goto auf dieselbe bereits aktive Cue löst keinen neuen Szenenaufruf aus. Tracking und andere Cue-Parts werden nicht automatisch aufgelöst.

## Programmer und mehrere Stationen

Für Schreibschritte wird nur das betreffende Fixture verwendet. Andere Programmer-Werte bleiben erhalten. Ist das benötigte Fixture bereits im Programmer aktiv, wartet der Abgleich bzw. verweigert den Schreibschritt. Auswahl und Gruppengitter werden über eine kurzzeitig belegte freie Gruppe im Bereich 9000 bis 9999 gesichert und wiederhergestellt. Es wird kein ClearAll ausgeführt.

Die Bridge auf einer vorgesehenen Station betreiben. Eine showweite Senderkennung verhindert gewöhnliche Mehrfachstarts. Prüfen / Diagnose bietet eine ausdrückliche Senderfreigabe an. Eine freigegebene Station stellt ihre Ausgabe beim nächsten Timerlauf ein. Die Sperre ist kein nachgewiesen atomarer Schutz gegen exakt gleichzeitige Starts auf mehreren Stationen.

## Prüfstand

Elf Gruppen mit dem tatsächlichen Lua-5.4-Produktionscode und MA-API-/UI-Testadaptern sind bestanden. Der öffentliche Import-Einstieg wird zusätzlich ohne Testexport geprüft, einschließlich absichtlich verweigerter MA-Funktionen während des Imports und der anschließenden Fehlerausgabe. Ein zusätzlicher strenger Compilecheck prüft alle 65 Schleifen auf Schreibzugriffe auf ihre Steuervariable. Die Textfeldprüfung kontrolliert Schriftart, UTF-8-Zeilenumbruch, geplante Breite, Zeilenhöhe und Abstände bei 1280, 960 und 800 Pixeln Bildschirmbreite. Die tatsächlichen MA-Schriftmaße müssen auf dem Pult und WebRemote geprüft werden. Native Darstellung, WebRemote, Import, Timer beim Showladen und reale DMX-Ausgabe müssen auf grandMA3 geprüft werden. Die Oberfläche wurde hier nicht auf einer echten MA ausgeführt.

- REFERENZEN.md: API-Grundlagen
- tests/: reproduzierbare Lua-Tests
- source/medialink_core.lua: wartbarer Programmcode
- tools/build_plugin.py: erzeugt die eigenständige Lua-Importdatei; --check prüft deren exakten Inhalt

Zum Import sind weiterhin nur XML und Lua aus MA3/ erforderlich. Weitere Dateien werden auf dem Pult nicht nachgeladen.
