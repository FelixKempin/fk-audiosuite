# Unter Windows bauen und testen

Fertige Installer gibt es auf der Download-Seite und als Artefakt des Workflows „Release“. Diese
Anleitung ist für alle, die FK Audio Suite unter Windows selbst bauen oder daran entwickeln.

## 1. Werkzeuge installieren

In einer **PowerShell**:

```powershell
winget install --id Git.Git -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id JRSoftware.InnoSetup -e      # nur für den Installer
```

- Die **Build Tools** bringen MSVC, das Windows-SDK und CMake mit (mehrere GB). Wer eine IDE möchte,
  nimmt stattdessen Visual Studio Community mit „Desktopentwicklung mit C++“.
- Danach **ein neues Fenster** öffnen, damit `git` gefunden wird.

**Lange Pfade erlauben** (die Submodule von JUCE und Tracktion haben tiefe Pfade). PowerShell **als
Administrator**:

```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
```

Dann als normaler Nutzer:

```powershell
git config --global core.longpaths true
git config --global core.autocrlf false          # Zeilenenden regelt .gitattributes im Repo (LF)
```

## 2. Repo holen

Am besten in einen kurzen Pfad und **nicht** in einen synchronisierten Ordner (OneDrive o. Ä.):
Build-Ordner sind groß und ändern sich ständig.

```powershell
git clone --recurse-submodules https://github.com/FelixKempin/fk-audiosuite.git FKAudioSuite
```

## 3. Bauen und testen

In der **„Developer PowerShell for VS 2022“** (Startmenü): Nur dort kennt die Shell MSVC und das
CMake der Build Tools.

```powershell
cd FKAudioSuite
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
& ".\build\windows-debug\src\app\FKAudioSuite_artefacts\Debug\FK Audio Suite.exe"
```

Wird der Speicher beim Bauen knapp, mit `--parallel 4` bremsen. Installer und Zip baut
`dist\release-windows.ps1` (`dist/README.md` §4).

## 4. Aktuell bleiben

```powershell
git pull
git submodule update --init --recursive
```

## 5. Installer testen

1. `…-windows-x64-setup.exe` starten. Solange die Installer nicht signiert sind, warnt SmartScreen:
   „Weitere Informationen“ → „Trotzdem ausführen“.
2. Audio-Einstellungen: WASAPI, DirectSound und ASIO (mit installiertem ASIO-Treiber) wählbar.
3. Projekt anlegen, speichern, laden; FK Rig Lite steht im Plugin-Browser.
4. Deinstallieren über die Windows-Einstellungen; Einstellungen und Presets bleiben.
