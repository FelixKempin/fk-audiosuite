# FK Audio Suite – Pakete bauen

Wie aus dem Quellcode die Pakete für **Linux** (AppImage) und **Windows** (Installer und Zip)
werden. Jedes Betriebssystem baut seine eigenen Pakete; die Schritte sind auf beiden Seiten gleich:
**mitgelieferte Plugins holen → DAW bauen und testen → packen**, und beides ist **ein** Befehl:

```bash
dist/release-linux.sh                                              # Linux
powershell -ExecutionPolicy Bypass -File .\dist\release-windows.ps1  # Windows (oder dist\windows\Release-Windows.cmd)
```

Ergebnis in `dist/out/`. Die offiziellen Pakete baut der Workflow „Release“ (`.github/workflows/release.yml`)
mit genau diesen Skripten; veröffentlicht werden sie außerhalb dieses Repos.

Mitgeliefert wird **FK Rig Lite** (VST3) als fertiges Release-Paket, festgeschrieben mit Prüfsumme
in `dist/bundled-plugins.lock` (ADR 0008 §3). Der Quellcode des Plugins ist nicht Teil dieses Repos.

---

## 1. Version

Die **einzige** Quelle der Version ist `project(... VERSION x.y.z)` in der obersten `CMakeLists.txt`.
App (Über-Dialog, Update-Vergleich), Dateinamen, Installer und Manifeste lesen sie von dort. Der
Git-Stand (kurzer SHA) landet im Build und im Manifest.

## 2. Einstellungen

Alle optional, als Umgebungsvariable oder in `dist/dist.env` (Schlüssel=Wert, nicht im Git):

| Name | Vorgabe | Bedeutung |
|---|---|---|
| `FK_RELEASE_CHANNEL` | `beta` | Kanal im Über-Dialog und im Manifest (`beta`, `stable`) |
| `FK_BUILD_JOBS` | `3` | parallele Compiler-Jobs (Linux); Tracktion braucht pro Job viel Speicher |
| `FK_UPDATE_URL` | Release-Liste der Website | woher die App Update-Hinweise holt (fest im Build) |

## 3. Linux

```bash
dist/release-linux.sh                # alles
dist/release-linux.sh --skip-tests   # ohne Tests
dist/release-linux.sh --keep-plugins # eigene VST3-Bundles aus dist/out/plugins/ statt der Pakete
```

`release-linux.sh` ruft der Reihe nach (einzeln aufrufbar):

```bash
dist/check-open-source.sh   # nur DAW-Code im Repo (läuft auch in der CI)
dist/fetch-plugins.sh       # Pakete laut bundled-plugins.lock laden, SHA-256 prüfen -> dist/out/plugins/
dist/build-linux.sh         # Release-Build (build/linux-release), danach alle Tests
dist/package-linux.sh       # packt das AppImage samt Plugins
```

Ergebnis: `FK-Audio-Suite-<Version>-linux-x64.AppImage` (+ `.sha256`) mit `usr/lib/vst3/FK Rig.vst3`,
dazu `release-linux-x64.json`. `package-linux.sh` lädt beim ersten Lauf `linuxdeploy` nach
`dist/tools/` (nicht im Git). Das AppImage enthält die App und die nötigen Bibliotheken; ALSA, JACK,
PipeWire und X11 kommen vom System. MP3-Export nutzt eine installierte `libmp3lame`, falls vorhanden.

Beim ersten Start einer neuen Version kopiert die App die mitgelieferten Plugins nach `~/.vst3` und
macht sie bekannt (`services::BundledPlugins`); dort finden sie auch andere DAWs. Ein Bundle, das der
Nutzer selbst dorthin gelegt hat (ohne Kennzeichen `.fk-bundled`), bleibt unangetastet.

Starttest: `chmod +x dist/out/FK-Audio-Suite-*-linux-x64.AppImage` und starten.

## 4. Windows

Nötig: **Visual Studio 2022** (oder Build Tools) mit „Desktopentwicklung mit C++“ (MSVC v143,
Windows-SDK, CMake) und für den Installer **Inno Setup 6** (ohne gibt es nur das Zip).
`release-windows.ps1` prüft beides und bietet Fehlendes über **winget** an.

```powershell
powershell -ExecutionPolicy Bypass -File .\dist\release-windows.ps1   # Schalter: -SkipTests -KeepPlugins
```

Einzelschritte wie auf Linux: `fetch-plugins.ps1`, `build-windows.ps1`, `package-windows.ps1`.

Ergebnis in `dist\out\`: `FK-Audio-Suite-<Version>-windows-x64-setup.exe` (Installer),
`FK-Audio-Suite-<Version>-windows-x64.zip` (portabel), je eine `.sha256`, dazu
`release-windows-x64.json`. Beide enthalten `VST3\FK Rig.vst3` neben dem Programm; der Installer legt
es auf Wunsch zusätzlich nach `C:\Program Files\Common Files\VST3` für andere DAWs. Audio unter
Windows: WASAPI, DirectSound und ASIO.

**Signieren** (freiwillig) übernimmt `windows/sign.ps1` mit Microsoft Trusted Signing, wenn die
Variablen `FK_SIGN_ENDPOINT`, `FK_SIGN_ACCOUNT`, `FK_SIGN_PROFILE` und die Anmeldung
(`AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`) gesetzt sind. Fehlen sie oder schlägt
das Signieren fehl, wird unsigniert weitergebaut.

## 5. CI (GitHub Actions)

- `ci.yml` baut bei jedem Push auf `main` unter Linux (Preset `ci-linux`) und führt alle Tests aus;
  Windows von Hand (Run workflow, Schalter `windows`).
- `release.yml` baut von Hand oder per Tag `fk-audio-suite/<version>` die Pakete für Linux
  (ubuntu-22.04, GCC 13, glibc ≥ 2.35, libstdc++ statisch) und Windows (MSVC, Inno Setup). Ergebnis
  als Artefakte `linux-x64` und `windows-x64` am Lauf.

MSVC-Warnungen sind vorerst keine Fehler. Sobald der Windows-Build sauber ist:
`-DFK_MSVC_WARNINGS_AS_ERRORS=ON` im Preset `windows-base` setzen.

## Dateien in diesem Ordner

| Datei | Zweck |
|---|---|
| `lib.sh`, `windows/lib.ps1` | gemeinsame Werte: Version, Kanal, Git-Stand, Dateinamen, Update-Adresse |
| `release-linux.sh`, `release-windows.ps1` | ganzer Lauf: Plugins holen, bauen, testen, packen |
| `check-open-source.sh` | prüft, dass nur DAW-Code im Repo liegt |
| `bundled-plugins.lock` | mitgelieferte Plugins: Pfad des Release-Pakets und SHA-256 je Plattform |
| `fetch-plugins.sh`, `fetch-plugins.ps1` | lädt die Pakete, prüft die Summe, legt die VST3-Bundles nach `dist/out/plugins/` |
| `build-linux.sh`, `build-windows.ps1` | Release-Build mit Kanal und Update-Adresse, danach Tests |
| `package-linux.sh`, `package-windows.ps1` | AppImage bzw. Installer + Zip, Prüfsummen, Manifest |
| `linux/fk-audio-suite.desktop` | Starter-Eintrag im AppImage |
| `windows/fk-audio-suite.iss` | Inno-Setup-Skript für den Installer |
| `windows/sign.ps1` | Signieren mit Trusted Signing, wenn eingerichtet |
| `windows/Release-Windows.cmd` | Doppelklick-Start von `release-windows.ps1` |
