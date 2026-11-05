# OBS v0.46 – Stand und nächste Schritte

Im Testbuild umgesetzt:

- Universum **1** als Standard für eine frische MediaLink-Einrichtung.
- MA-Plugin und beide Fixtures zusätzlich lokal mit dem Windows-Setup ablegen.
- Den lokalen MA-Paketordner aus dem MediaLink-Einstellungsfenster öffnen können.
- Die vom Setup verwalteten lokalen MA-Dateien bei Updates durch die passenden Versionen ersetzen. Der Import in grandMA3 bleibt ein eigener Vorgang.
- Das freigegebene Logo für Über-Tab, Fenster und Windows-Setup/Deinstallationseintrag.
- Ein GitHub-Repository-Link im Über-Tab.
- Automatische Multicast-Vorgabe und Auswahl einer lokalen Netzwerkkarte.
- Optionale Löschung der MediaLink-Einstellungen bei der Deinstallation, standardmäßig ausgeschaltet.

Als Nächstes wird v0.46 praktisch getestet:

1. Frische Einrichtung: Universum 1 und „Automatisch (Windows-Standard)“ prüfen.
2. Update von v0.45: bestehendes Universum, feste Interface-IP und Regeln müssen erhalten bleiben.
3. MA-Paketordner öffnen, HTML-Startseite lesen und die XML-/Lua-/GDTF-Dateien prüfen.
4. Deinstallation mit ausgeschalteter Löschoption: Regeln und Einstellungen müssen bei Neuinstallation erhalten bleiben.
5. Deinstallation mit eingeschalteter Löschoption: ausschließlich die MediaLink-Einstellungen des angezeigten Kontos müssen verschwinden. Anschließend müssen die frischen Vorgaben wieder gelten.
6. Multicast mit automatischer und gezielt ausgewählter Schnittstelle im eigenen Netzwerk prüfen.
7. Szenenwechsel, Bild-/Tonfades, Blackmagic und OBS-Shutdown prüfen; anschließend Installation auf dem Rechner ohne Entwicklerwerkzeuge wiederholen.

Nach diesen Tests folgt ein neuer Release-Entwurf für v0.46.0-beta.1 mit dem zugehörigen Quellstand und Prüfsummen. Die veröffentlichte Beta v0.45 bleibt unverändert verfügbar. Die festgelegten Dateien unter `releases/beta.json` gehören weiterhin zur bisherigen v0.45 und werden erst für eine neue Release-Vorbereitung bewusst aktualisiert.
