#!/usr/bin/env bash
# Baut FK Audio Suite für Linux im Release-Modus und lässt die Tests laufen.
# Ergebnis: build/linux-release/src/app/FKAudioSuite_artefacts/Release/FK Audio Suite
#
# Aufruf:  dist/build-linux.sh [--skip-tests]
# Kanal, Jobs, Update-Adresse: Umgebung oder dist/dist.env (dist/lib.sh).
source "$(dirname "$0")/lib.sh"

SKIP_TESTS=0
[[ "${1:-}" == "--skip-tests" ]] && SKIP_TESTS=1

echo "==> Linux-Build  Version ${FK_VERSION}, Kanal ${FK_RELEASE_CHANNEL}, Stand ${FK_GIT_SHA}"
cmake --preset linux-release \
  -DFK_RELEASE_CHANNEL="${FK_RELEASE_CHANNEL}" \
  -DFK_GIT_SHA="${FK_GIT_SHA}" \
  -DFK_UPDATE_URL="$(fk_update_url)"
cmake --build --preset linux-release -j "${FK_BUILD_JOBS}"

if [[ "$SKIP_TESTS" == 0 ]]; then
  echo "==> Tests"
  # Ohne Bildschirm (z. B. per SSH) brauchen die Oberflächen-Tests einen virtuellen: xvfb-run.
  if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]] && command -v xvfb-run >/dev/null; then
    xvfb-run -a build/linux-release/tests/fk_tests
  else
    build/linux-release/tests/fk_tests
  fi
fi

echo "==> Fertig: build/linux-release/src/app/FKAudioSuite_artefacts/Release/FK Audio Suite"
