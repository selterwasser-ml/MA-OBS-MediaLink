# Beta v0.45 / MA v0.42 vorbereiten

## Festgelegter Stand

- Release-Name: **MA OBS MediaLink v0.45 BETA / grandMA3 v0.42**.
- Vorgesehener Tag: **v0.45.0-beta.1**.
- Binär- und Quellstand: `e35e3c71f41e557844a2147ac098c66fab537fd7`.
- Erfolgreicher Build: [41978787624](https://github.com/selterwasser-ml/MA-OBS-MediaLink/actions/runs/41978787624).
- Von mir getesteter Aufbau: **OBS 33.0.1**, **grandMA3 2.5.0.3**.
- Installation auf einem zweiten Rechner ohne Entwicklerwerkzeuge: erfolgreich getestet.

`releases/beta.json` hält den Build, das Artefakt und dessen SHA256 fest. Die Versionshinweise liegen unter `releases/v0.45.0-beta.1.md`. Die Release-Dateien werden aus dem vorhandenen getesteten Build übernommen. Neue Builds auf `main` ersetzen diesen festgelegten Stand nicht automatisch.

## Entwurf erstellen

Der Workflow **Beta-Entwurf vorbereiten** läuft bei Änderungen seiner festgelegten Eingabedateien auf `main` oder manuell über **Actions → Beta-Entwurf vorbereiten → Run workflow**. Er:

1. prüft Build-Ergebnis, Quellstand, Artefakt-Prüfsumme und die einzelnen Paketprüfsummen;
2. prüft die Buildinformationen und den DLL-Hash im Windows-ZIP;
3. ergänzt den passenden vollständigen Repository-Quellstand und eine Prüfsummendatei für alle vier Downloadpakete;
4. erstellt einen **Draft** mit **Pre-release**-Kennzeichnung und fünf Anhängen;
5. liest den gespeicherten Entwurf zurück und prüft die Anhänge;
6. stellt die vorbereiteten Dateien zusätzlich als Actions-Artefakt bereit.

Ein vorhandener identischer Entwurf kann vervollständigt werden. Textkorrekturen lassen sich über `previousNotesSha256` in `releases/beta.json` gezielt für den bisherigen Textstand freigeben. Weicht der gespeicherte Text davon ab, hält der Workflow an, damit zwischenzeitliche manuelle Änderungen erhalten bleiben. Bereits veröffentlichte Releases und abweichende Anhänge werden nicht überschrieben. Der Workflow veröffentlicht nichts, ändert keine Repository-Sichtbarkeit und tauscht keine alten Releases automatisch aus.

Die nur für diesen Workflow benötigten Rechte sind `actions: read` und `contents: write`. Der vorhandene Windows-Build behält seine Leserechte. Eigene dauerhafte Zugangsschlüssel werden nicht benötigt.

## Fertige Dateien

- `MA_OBS_MediaLink_v045_Setup_x64.exe`
- `MA_OBS_MediaLink_v045_Windows_x64.zip`
- `MA_OBS_MediaLink_MA042.zip`
- `MA_OBS_MediaLink_v045_MA042_Source.zip`
- `SHA256SUMS.txt`

Das Actions-Artefakt enthält zusätzlich Versionshinweise und `RELEASE-INFO.json` mit den geprüften Herkunftsangaben. Die Projektlizenz und das Foto-Copyright bleiben unverändert.

## Veröffentlichung

Nach Prüfung des Entwurfs ist das Veröffentlichen ein eigener Schritt. Die **Pre-release**-Kennzeichnung bleibt gesetzt. Solange das Repository privat ist, sind auch veröffentlichte Releases nur für Personen mit Repository-Zugriff zugänglich. Eine öffentliche Freigabe oder Änderung der Sichtbarkeit wird durch die Vorbereitung nicht vorgenommen.

Die Wünsche aus [NEXT_STEPS.md](NEXT_STEPS.md) sind noch nicht implementiert und ändern diesen getesteten Build nicht.
