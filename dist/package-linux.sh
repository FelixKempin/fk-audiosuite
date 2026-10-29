#!/usr/bin/env bash
# Baut und verpackt FK Audio Suite als AppImage: eine Datei, läuft auf den üblichen Distributionen
# ohne Installation (chmod +x, Doppelklick). Dazu die Prüfsumme für die Website.
#
# Aufruf:  dist/package-linux.sh [--skip-tests]
# Ergebnis:
#   dist/out/FK-Audio-Suite-<Version>-linux-x64.AppImage
#   dist/out/FK-Audio-Suite-<Version>-linux-x64.AppImage.sha256
#   dist/out/release-linux-x64.json   (Angaben für Website und Update-Hinweis)
source "$(dirname "$0")/lib.sh"

dist/build-linux.sh "${1:-}"

BINARY="build/linux-release/src/app/FKAudioSuite_artefacts/Release/FK Audio Suite"
APPDIR="${FK_OUT}/AppDir"
mkdir -p "${FK_OUT}" dist/tools
rm -rf "${APPDIR}"

# AppDir nach freedesktop-Muster: Programm, Starter, Symbol, Lizenz.
install -Dm755 "${BINARY}" "${APPDIR}/usr/bin/fk-audio-suite"
install -Dm644 dist/linux/fk-audio-suite.desktop "${APPDIR}/usr/share/applications/fk-audio-suite.desktop"
install -Dm644 resources/icons/fk-audio-suite.png "${APPDIR}/usr/share/icons/hicolor/512x512/apps/fk-audio-suite.png"
install -Dm644 LICENSE "${APPDIR}/usr/share/doc/fk-audio-suite/LICENSE" 2>/dev/null || true
install -Dm644 docs/THIRD_PARTY.md "${APPDIR}/usr/share/doc/fk-audio-suite/THIRD_PARTY.md"

# Mitgelieferte Lite-Plugins (dist/fetch-plugins.sh). Die App kopiert sie beim ersten Start nach
# ~/.vst3 (services::BundledPlugins), weil das AppImage bei jedem Start woanders eingehängt ist.
if compgen -G "${FK_OUT}/plugins/*.vst3" >/dev/null; then
  mkdir -p "${APPDIR}/usr/lib/vst3"
  cp -a "${FK_OUT}/plugins/"*.vst3 "${APPDIR}/usr/lib/vst3/"
  echo "==> Mitgeliefert: $(cd "${FK_OUT}/plugins" && ls -d *.vst3 | tr '\n' ' ')"
else
  echo "Hinweis: keine Plugins in ${FK_OUT}/plugins (dist/fetch-plugins.sh), AppImage ohne Lite-Plugins."
fi

# linuxdeploy holt die nötigen Bibliotheken (freetype, libstdc++ …) ins AppImage. Es wird einmal
# heruntergeladen und in dist/tools/ abgelegt (nicht im Git). Feste Version mit Prüfsumme, nicht
# "continuous": sonst kämen ungeprüft andere Werkzeuge ins Paket.
LINUXDEPLOY="dist/tools/linuxdeploy-1-alpha-20251107-1-x86_64.AppImage"
if [[ ! -x "${LINUXDEPLOY}" ]]; then
  echo "==> linuxdeploy laden"
  mkdir -p dist/tools
  curl -fL --retry 3 -o "${LINUXDEPLOY}.part" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
  echo "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d  ${LINUXDEPLOY}.part" | sha256sum -c --quiet \
    || { rm -f "${LINUXDEPLOY}.part"; echo "linuxdeploy: Prüfsumme stimmt nicht" >&2; exit 1; }
  mv "${LINUXDEPLOY}.part" "${LINUXDEPLOY}"
  chmod +x "${LINUXDEPLOY}"
fi

APPIMAGE="${FK_OUT}/${FK_ARTIFACT_BASE}.AppImage"
rm -f "${APPIMAGE}"
echo "==> AppImage bauen"
# APPIMAGE_EXTRACT_AND_RUN: geht auch ohne FUSE (Container, CI).
APPIMAGE_EXTRACT_AND_RUN=1 OUTPUT="${APPIMAGE}" VERSION="${FK_VERSION}" \
  "${LINUXDEPLOY}" --appdir "${APPDIR}" \
  --desktop-file "${APPDIR}/usr/share/applications/fk-audio-suite.desktop" \
  --icon-file "${APPDIR}/usr/share/icons/hicolor/512x512/apps/fk-audio-suite.png" \
  --output appimage

( cd "${FK_OUT}" && sha256sum "$(basename "${APPIMAGE}")" > "$(basename "${APPIMAGE}").sha256" )
rm -rf "${APPDIR}"

cat > "${FK_OUT}/release-${FK_PLATFORM}.json" <<JSON
{
  "version": "${FK_VERSION}",
  "channel": "${FK_RELEASE_CHANNEL}",
  "platform": "${FK_PLATFORM}",
  "gitSha": "${FK_GIT_SHA}",
  "date": "$(date -u +%Y-%m-%d)",
  "file": "$(basename "${APPIMAGE}")",
  "sha256": "$(cut -d' ' -f1 "${APPIMAGE}.sha256")",
  "size": $(stat -c %s "${APPIMAGE}")
}
JSON

# Manifest fk-release-manifest/1 je Datei (dist/write-manifest.py); nur für echte Reifestufen.
case "${FK_RELEASE_CHANNEL}" in
  alpha|beta|stable)
    python3 dist/write-manifest.py --out "${FK_OUT}" --file "$(basename "${APPIMAGE}")" \
      --product fk-audio-suite --family fk-audio --version "${FK_VERSION}" --track "${FK_RELEASE_CHANNEL}" \
      --os linux --arch x86_64 --package appimage --os-min "glibc 2.35" ;;
esac

echo
echo "==> Paket:   ${APPIMAGE}"
echo "    Testen:  chmod +x \"${APPIMAGE}\" && \"${APPIMAGE}\""
