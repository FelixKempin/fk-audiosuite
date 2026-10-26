#!/usr/bin/env bash
# Die ganze Linux-Auslieferung in einem Befehl: Lite-Plugins holen, DAW bauen und testen, AppImage
# packen. Ergebnis in dist/out/ (AppImage, .sha256, release-linux-x64.json).
#
# Aufruf:  dist/release-linux.sh [--skip-tests] [--keep-plugins]
#   --keep-plugins  die VST3-Bundles in dist/out/plugins/ nehmen, wie sie sind, statt sie laut
#                   dist/bundled-plugins.lock zu laden (z. B. selbst gebaute)
source "$(dirname "$0")/lib.sh"

SKIP_TESTS="" KEEP_PLUGINS=0
for arg in "$@"; do
  case "$arg" in
    --skip-tests) SKIP_TESTS="--skip-tests" ;;
    --keep-plugins) KEEP_PLUGINS=1 ;;
    *) echo "Unbekannte Option: $arg" >&2; exit 2 ;;
  esac
done

started=$(date +%s)
echo "==> FK Audio Suite ${FK_VERSION} (${FK_RELEASE_CHANNEL}), Stand ${FK_GIT_SHA}"
if [[ -n "$(git status --porcelain 2>/dev/null)" ]]; then
  echo "WARNUNG: ungespeicherte Änderungen im Arbeitsverzeichnis; der Build trägt trotzdem ${FK_GIT_SHA}." >&2
fi

dist/check-open-source.sh
if [[ "$KEEP_PLUGINS" == 0 ]]; then
  dist/fetch-plugins.sh
fi
dist/package-linux.sh $SKIP_TESTS

echo "==> Fertig nach $(( ($(date +%s) - started) / 60 )) min. Pakete in ${FK_OUT}/"
