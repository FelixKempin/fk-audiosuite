# Baut FK Audio Suite für Windows (MSVC, Visual Studio 2022) im Release-Modus und lässt die Tests laufen.
# Ergebnis: build\windows-release\src\app\FKAudioSuite_artefacts\Release\FK Audio Suite.exe
#
# Aufruf (PowerShell im Projektordner):  .\dist\build-windows.ps1 [-SkipTests]
# Voraussetzungen: dist/README.md §2 (Visual Studio 2022 mit "Desktopentwicklung mit C++", CMake, Ninja nicht nötig).
param(
    [switch]$SkipTests
)
. (Join-Path $PSScriptRoot 'windows\lib.ps1')
$null = Use-FkCmake

Write-Host "==> Windows-Build  Version $FkVersion, Kanal $FkChannel, Stand $FkGitSha"
$configureArgs = @(
    '--preset', 'windows-release',
    "-DFK_RELEASE_CHANNEL=$FkChannel",
    "-DFK_GIT_SHA=$FkGitSha",
    "-DFK_UPDATE_URL=$FkUpdateUrl"
)

cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake-Konfiguration fehlgeschlagen' }

cmake --build --preset windows-release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }

if (-not $SkipTests) {
    Write-Host '==> Tests'
    & (Join-Path $FkRoot 'build\windows-release\tests\Release\fk_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Tests fehlgeschlagen' }
}

Write-Host '==> Fertig: build\windows-release\src\app\FKAudioSuite_artefacts\Release\FK Audio Suite.exe'
