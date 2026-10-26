#!/usr/bin/env bash
# Gemeinsame Helfer der Linux-dist-Skripte (bauen und packen). Wird per `source` geladen, nicht
# direkt ausgeführt.
set -euo pipefail

FK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$FK_ROOT"

# Einstellungen: dist/dist.env, falls vorhanden (nicht im Git); Umgebungsvariablen haben Vorrang.
# Bekannt sind FK_RELEASE_CHANNEL, FK_BUILD_JOBS und FK_UPDATE_URL.
if [[ -f dist/dist.env ]]; then
  while IFS='=' read -r key value; do
    [[ -z "$key" || "$key" == \#* ]] && continue
    [[ -z "${!key:-}" ]] && export "$key=$value"
  done < dist/dist.env
fi

FK_RELEASE_CHANNEL="${FK_RELEASE_CHANNEL:-beta}"
# Parallele Compiler-Jobs. Tracktion und JUCE brauchen pro Job viel Speicher; mehr Jobs nur mit
# reichlich RAM.
FK_BUILD_JOBS="${FK_BUILD_JOBS:-3}"

# Version: einzige Quelle ist project(VERSION) in CMakeLists.txt.
FK_VERSION="$(sed -n 's/^  VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"

# Git-Stand: aus git, sonst aus .fk-git-sha (Kopie ohne git).
FK_GIT_SHA="$(git rev-parse --short HEAD 2>/dev/null || cat .fk-git-sha 2>/dev/null || echo unbekannt)"

FK_PLATFORM="linux-x64"
FK_OUT="dist/out"
FK_ARTIFACT_BASE="FK-Audio-Suite-${FK_VERSION}-${FK_PLATFORM}"

# Woher die App Update-Hinweise holt (fest im Build): die öffentliche Release-Liste der Website.
fk_update_url() {
  echo "${FK_UPDATE_URL:-https://licensing.fk-onlineservices.de/v1/releases/?product=fk-audio-suite}"
}
