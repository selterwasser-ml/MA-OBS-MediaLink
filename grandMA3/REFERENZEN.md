# Technische Referenzen

Offizielle MA-Dokumentation, abgerufen am 3. November 2026. Die Implementierung muss zusätzlich auf der eingesetzten Version geprüft werden.

- MessageBox, Eingaben, Selektoren und Rückgabewerte: https://help.malighting.com/grandMA3/2.1/HTML/lua_objectfree_messagebox.html
- Globale Variablen: https://help.malighting.com/grandMA3/2.0/HTML/lua_variables.html
- Plugin-Komponenten und Aufrufsyntax: https://help.malighting.com/grandMA3/2.3/HTML/plugins.html
- Timer: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_timer.html
- GetUIChannels, Objektkanalindex und SUBATTRIBUTE: https://help.malighting.com/grandMA3/2.1/HTML/lua_objectfree_getuichannels.html
- GetRTChannel, ui_index_first und Coarse/Fine/Ultra-Patch: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_getrtchannel.html
- GetPresetData und by_fixtures: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_getpresetdata.html
- Store-Option /ActiveForSelected: https://help.malighting.com/grandMA3/2.3/HTML/ok_activeforselected.html
- Store-Option /KeepActivation: https://help.malighting.com/grandMA3/2.3/HTML/ok_keepactivation.html
- Preset-Pools einschließlich der fünf All-Pools: https://help.malighting.com/grandMA3/2.5/HTML/presets_pools.html
- GetCurrentCue bezieht sich auf die ausgewählte Sequenz und wird deshalb hier nicht verwendet: https://help.malighting.com/grandMA3/2.0/HTML/lua_objectfree_getcurrentcue.html
- onPC-RUNPLUGIN und Komponentennummer: https://help.malighting.com/grandMA3/2.5/HTML/shf_organize.html

Der feste Sequenzzugriff über CurrentChild wird durch eine Laufzeitprüfung abgesichert; eine GetCurrentCue-Rückfalllösung würde die gewünschte Unabhängigkeit von der selektierten Sequenz verletzen.

- Data Pools, Standard-Datenpool und für Preview reservierter Pool 128: https://help.malighting.com/grandMA3/2.5/HTML/datapool.html
- Sperren mit verschiedenen Ebenen: https://help.malighting.com/grandMA3/2.5/HTML/ok_recursive.html

Die spezielle Nullbasis der RT-Patchwerte wird durch Regressionstests der internen Adressrechnung behandelt. Der Dokumentationsabschnitt nennt die Patchfelder, legt dort jedoch nicht ausdrücklich deren Indexbasis fest. Die Werte der betroffenen Konsole sind zusätzlich im Vor-Ort-Test zu bestätigen.

- GetVar und Verhalten fehlender Variablen: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_getvar.html
- SetVar und Erfolgsrückgaben: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_setvar.html
- DelVar und bereits fehlende Variablen: https://help.malighting.com/grandMA3/2.3/HTML/lua_objectfree_delvar.html

Die Dokumentation bezeichnet den fehlenden Wert als nil. Das Nutzerprotokoll und die reproduzierte Lua-Aufrufstelle zeigen, dass der Wrapper auch null Rückgabewerte sicher behandeln muss. v0.33 normalisiert beides an derselben Stelle.

- Monotone Stationslaufzeit Time(), float in Sekunden: https://help.malighting.com/grandMA3/2.1/HTML/lua_objectfree_time.html
- Decimal16-/Decimal24-Lesebereiche: https://help.malighting.com/grandMA3/2.5/HTML/fixture-sheet-settings.html

Die Rückleseprüfung verwendet für Szene/Token die oberen 16 Bits des internen 24-Bit-Phaserwerts. Der zugehörige Testadapter speichert auch Decimal16-Eingaben mit der internen 24-Bit-Skalierung; ein echter MA-/DMX-Livetest bleibt erforderlich.

Die Namenskorrektur v0.35 beruht unmittelbar auf dem vom Nutzer gelieferten MA-System-Monitor-Auszug: Der Cue-Name mit Kommas wird dort ausdrücklich durch die MA in eine kommafreie Zeichenfolge geändert. Die neue Implementierung verwendet ASCII-Buchstaben, Ziffern und Leerzeichen und prüft bereits vorhandene geänderte Namen zusammen mit den gespeicherten Werten.

- Offiziell behobene Lua-Fehler beim Öffnen des Sequenzeditors in 2.5.1.0: https://help.malighting.com/grandMA3/2.5/HTML/rn_bug_fix_version-2-5-1-0.html
- BuildDetails und BigVersion: https://help.malighting.com/grandMA3/2.0/HTML/lua_objectfree_builddetails.html

## UI und Sequenzidentität v0.39 – 4. November 2026

- Sequenzeinstellungen mit den Eigenschaften Name und Note: https://help.malighting.com/grandMA3/2.5/HTML/cue_sequence_settings.html
- Objekt:Set und die ausdrücklich fehlende Rückgabe: https://help.malighting.com/grandMA3/2.4/HTML/lua_object_set.html
- GetFocusDisplay: https://help.malighting.com/grandMA3/2.0/HTML/lua_objectfree_getfocusdisplay.html
- IsObjectValid: https://help.malighting.com/grandMA3/2.5/HTML/lua_objectfree_isobjectvalid.html
- Native BaseInput-/TitleBar-/DialogFrame-/Button-Beispiele des jeweiligen Autors: https://forum.malighting.com/forum/thread/7623-lua-function-for-connected-drives/?postID=87145
- UI-Warteschleife und IsObjectValid im Originalbeispiel: https://forum.malighting.com/forum/thread/5738-lua-ui/?postID=20391
- Fokusumleitung der Please-Taste auf native Buttons: https://forum.malighting.com/forum/thread/5738-lua-ui/?postID=84001

Die benutzerdefinierten UI-Klassen werden nicht vollständig im MA-Handbuch dokumentiert. Ihre Verwendung wurde anhand von Primärbeispielen ihrer Autoren entworfen und mit einem Testadapter geprüft; die reale Darstellung muss auf der eingesetzten MA-Version bestätigt werden. Der Standarddialog bleibt als Rückfall erhalten.

## Schreibgeschützte Schleifenvariablen v0.41

- Lua 5.4, for-Anweisungen und Hinweis, die Steuervariable nicht zu ändern: https://www.lua.org/manual/5.4/manual.html#3.3.5
- Lua 5.5, erste generische for-Variable und numerische for-Variable als const: https://www.lua.org/manual/5.5/manual.html#3.3.5

Die Korrektur folgt direkt aus der vom Nutzer gelieferten Compilerfehlermeldung an Kernzeile 120. Die konkrete Lua-Version der Konsole wurde nicht ermittelt. Der zusätzliche Compilecheck erzwingt die Read-only-Regel in einer temporären Prüfvariante unter dem vorhandenen Lua-5.4-Compiler.

## Direkte Cue-Werte v0.43 – 11. November 2026

- Store-Quellen, Active For Selected und Merge/Remove:
  https://help2.malighting.com/grandMA3/2.5/HTML/cue_store_settings_preferences.html
- Programmer als ausdrücklich gewählte Quelle:
  https://help.malighting.com/grandMA3/2.5/HTML/ok_programmer.html
- Entfernen aktiver Attributwerte aus einer Cue:
  https://help.malighting.com/grandMA3/2.4/HTML/ok_remove.html
- CueOnly Off; CueOnly On würde zusätzliche Blockwerte in Folge-Cues erzeugen:
  https://help.malighting.com/grandMA3/2.2/HTML/ok_cueonly.html
- TrackingShield Off:
  https://help.malighting.com/grandMA3/2.1/HTML/key_rn_v2_0.html

Der absolute Wert wird aktiv gesetzt und mit Merge in den ausdrücklich
adressierten Part 0 geschrieben. Individuelle Zeitschichten werden nicht
aktiviert. Das Verhalten wurde am Adapter geprüft und braucht den Praxistest
auf der eingesetzten Konsole; die Dokumentation ersetzt diesen nicht.
Der Anwender hat den Funktionstest am 11. November 2026 als erfolgreich bestätigt.
Historische Referenzen oben dokumentieren die Entwicklung vor v0.43.
