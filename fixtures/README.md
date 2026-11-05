# Fixtureprofile

Die beiden Profile stammen aus dem bisherigen Zwei-Fixture-Paket. Ihre Namen und Beschreibungen wurden überarbeitet; die Fixture-Typ-IDs und die Kanaldefinitionen bleiben erhalten. Beim Import in eine bestehende Show kann grandMA3 aufgrund der gleichen Typ-ID ein bereits vorhandenes Profil erkennen; das Verhalten bitte vor einem Austausch an einer Showkopie prüfen.

| Datei | Funktion | Attribute / relative DMX-Kanäle |
| --- | --- | --- |
| MA_OBS_MediaLink_Fade.gdtf | Fade-Fixture, 3 Kanäle | VideoCrossfade: 1–3, 24 Bit |
| MA_OBS_MediaLink_Steuerung.gdtf | Steuer-Fixture, 7 Kanäle | VideoSceneNumber: 1–2, 16 Bit; Dimmer als Endwert: 3–5, 24 Bit; VideoTransitionToken: 6–7, 16 Bit |

## Patch

Das Fade-Fixture auf die in OBS eingestellte Startadresse patchen. Das separate Steuer-Fixture beginnt drei Kanäle später. Beide müssen im selben Universum liegen. Der gesamte Zehn-Kanal-Block muss hineinpassen; die Startadresse kann 1 bis 503 sein.

Beispiel bei Universum 1 und Startadresse 1 (neue OBS-Standardeinstellung ab v0.46):

| Fixture | Startadresse | Belegung |
| --- | --- | --- |
| Fade-Fixture | 1.001 | 1.001–003 |
| Steuer-Fixture | 1.004 | 1.004–010 |

Die Fixture-IDs sind frei wählbar und werden unter Einrichtung im MA-Plugin angegeben. Die Profile selbst schreiben keine Fixture-ID vor. Die Diagnose prüft Attribute, Auflösung und Patch. Das Steuer-Fixture für MediaLink reservieren; der als Endwert verwendete Dimmer ist zusätzlich auf mögliche Master-/Blackout-Einflüsse in der konkreten Show zu prüfen.

Die Profile tragen dieselben Typ-IDs wie die bisherigen Dateien. Die umbenannten Profile müssen nach dem Import auf der eingesetzten grandMA3-Version geprüft werden.
