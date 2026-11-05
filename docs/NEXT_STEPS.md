# OBS v0.46 – Stand und nächste Schritte

In v0.46 umgesetzt und praktisch getestet:

- Universum **1** als Standard für eine frische MediaLink-Einrichtung.
- MA-Plugin und beide Fixtures zusätzlich lokal mit dem Windows-Setup ablegen.
- Den lokalen MA-Paketordner aus dem MediaLink-Einstellungsfenster öffnen können.
- Die vom Setup verwalteten lokalen MA-Dateien bei Updates durch die passenden Versionen ersetzen. Der Import in grandMA3 bleibt ein eigener Vorgang.
- Das freigegebene Logo für Über-Tab, Fenster und Windows-Setup/Deinstallationseintrag.
- Ein GitHub-Repository-Link im Über-Tab.
- Automatische Multicast-Vorgabe und Auswahl einer lokalen Netzwerkkarte.
- Optionale Löschung der MediaLink-Einstellungen bei der Deinstallation, standardmäßig ausgeschaltet.

Diese Testpunkte habe ich abgeschlossen:

1. Frische Einrichtung mit Universum 1 und „Automatisch (Windows-Standard)“.
2. Update von v0.45 mit Erhalt des bestehenden Universums, der festen Interface-IP und der Regeln.
3. MA-Paketordner, Offline-Startseite, XML-/Lua-/GDTF-Dateien, Logo und GitHub-Link.
4. Deinstallation ohne Löschung der Einstellungen und anschließende Neuinstallation.
5. Deinstallation mit Löschung ausschließlich der MediaLink-Einstellungen des angezeigten Kontos; danach frische Vorgaben bei Neuinstallation.
6. Multicast mit automatischer und gezielt ausgewählter Schnittstelle im eigenen Netzwerk.
7. Bisheriger Szenen-, Bild-/Ton-, Blackmagic- und OBS-Shutdown-Praxistest sowie Installation auf dem zweiten Rechner ohne Entwicklerwerkzeuge.

Als Nächstes wird der Entwurf für **v0.46.0-beta.1** mit dem getesteten Quellstand und allen fünf Release-Dateien geprüft. Anschließend kann ich ihn als Pre-release veröffentlichen. Die veröffentlichte Beta v0.45 bleibt unverändert verfügbar. `releases/current.json` legt den neuen Entwurf fest; `releases/beta.json` und der ältere Vorbereitungsworkflow bleiben als bisheriger v0.45-Stand erhalten.
