# Beta v0.46 / MA v0.42 vorbereiten

## Festgelegter Stand

- Release-Name: **MA OBS MediaLink v0.46 BETA / grandMA3 v0.42**.
- Vorgesehener Tag: **v0.46.0-beta.1**.
- Binär- und Quellstand: `3d21d5ffe65787880a4a045fad08d9dd748dc9a2`.
- Erfolgreicher Build: [42009314072](https://github.com/selterwasser-ml/MA-OBS-MediaLink/actions/runs/42009314072).
- Von mir getesteter Aufbau: **OBS 33.0.1**, **grandMA3 2.5.0.3**.
- Installation auf einem zweiten Rechner ohne Entwicklerwerkzeuge: erfolgreich getestet.

Die vorgesehenen praktischen Tests für v0.46 habe ich abgeschlossen. `releases/current.json` hält den Build, das Artefakt und dessen SHA256 fest. Die Versionshinweise liegen unter `releases/v0.46.0-beta.1.md`. Die Release-Dateien werden unverändert aus dem vorhandenen getesteten Build übernommen. Neue Builds auf `main` ersetzen diesen festgelegten Stand nicht automatisch.

## Entwurf erstellen

Der Workflow **Aktuellen Beta-Entwurf vorbereiten** läuft bei Änderungen seiner festgelegten Eingabedateien auf `main` oder manuell über **Actions → Aktuellen Beta-Entwurf vorbereiten → Run workflow**. Er:

1. prüft Build-Ergebnis, Quellstand, Artefakt-Prüfsumme und die einzelnen Paketprüfsummen;
2. prüft die Buildinformationen und den DLL-Hash im Windows-ZIP;
3. ergänzt den passenden vollständigen Repository-Quellstand und eine Prüfsummendatei für alle vier Downloadpakete;
4. erstellt einen **Draft** mit **Pre-release**-Kennzeichnung und fünf Anhängen;
5. liest den gespeicherten Entwurf zurück und prüft die Anhänge;
6. stellt die vorbereiteten Dateien zusätzlich als Actions-Artefakt bereit.

Ein vorhandener identischer Entwurf kann vervollständigt werden. Textkorrekturen lassen sich über `previousNotesSha256` in `releases/current.json` gezielt für den bisherigen Textstand freigeben. Weicht der gespeicherte Text davon ab, hält der Workflow an, damit zwischenzeitliche manuelle Änderungen erhalten bleiben. Bereits veröffentlichte Releases und abweichende Anhänge werden nicht überschrieben. Der Workflow veröffentlicht nichts, ändert keine Repository-Sichtbarkeit und tauscht keine alten Releases automatisch aus.

Die nur für diesen Workflow benötigten Rechte sind `actions: read` und `contents: write`. Der vorhandene Windows-Build behält seine Leserechte. Eigene dauerhafte Zugangsschlüssel werden nicht benötigt.

## Fertige Dateien

- `MA_OBS_MediaLink_v046_Setup_x64.exe`
- `MA_OBS_MediaLink_v046_Windows_x64.zip`
- `MA_OBS_MediaLink_MA042.zip`
- `MA_OBS_MediaLink_v046_MA042_Source.zip`
- `SHA256SUMS.txt`

Das Actions-Artefakt enthält zusätzlich Versionshinweise und `RELEASE-INFO.json` mit den geprüften Herkunftsangaben. Die Projektlizenz und das Foto-Copyright bleiben unverändert.

## Veröffentlichung

Nach Prüfung des Entwurfs ist das Veröffentlichen ein eigener Schritt. Die **Pre-release**-Kennzeichnung bleibt gesetzt. Solange das Repository privat ist, sind auch veröffentlichte Releases nur für Personen mit Repository-Zugriff zugänglich. Eine öffentliche Freigabe oder Änderung der Sichtbarkeit wird durch die Vorbereitung nicht vorgenommen.

Der [Stand der v0.46-Arbeiten](NEXT_STEPS.md) und die [Versionshinweise](../releases/v0.46.0-beta.1.md) dokumentieren die umgesetzten Änderungen und abgeschlossenen Tests. Die Dateien `releases/beta.json`, `releases/v0.45.0-beta.1.md`, `tools/prepare_beta_release.py` und der ältere Workflow **Beta-Entwurf vorbereiten** gehören weiterhin zur vorherigen v0.45. Sie werden für den neuen Entwurf nicht geändert oder ausgeführt.
