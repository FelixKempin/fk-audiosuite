#!/usr/bin/env bash
# Holt die mitgelieferten Lite-Plugins als fertige Release-Pakete (dist/bundled-plugins.lock), prüft
# die SHA-256 und legt die VST3-Bundles nach dist/out/plugins/. package-linux.sh packt sie ins
# AppImage. Ohne Zugriff auf das private Plugin-Repo, daher auch in der öffentlichen CI.
# Eigene Bundles statt der Pakete: nach dist/out/plugins/ legen und release-linux.sh --keep-plugins.
#
# Aufruf:  dist/fetch-plugins.sh
source "$(dirname "$0")/lib.sh"

LOCK="${FK_ROOT}/dist/bundled-plugins.lock"
PLUGINS_OUT="${FK_OUT}/plugins"
rm -rf "${PLUGINS_OUT}"
mkdir -p "${PLUGINS_OUT}"

base="$(awk '$1 == "base" { print $2 }' "$LOCK")"
[[ -n "$base" ]] || { echo "Keine base-Zeile in ${LOCK}." >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

found=0
while read -r platform path sha; do
  [[ "$platform" == linux ]] || continue
  found=1
  file="$(basename "$path")"
  echo "==> Plugin ${file}"
  curl -fsSL --retry 3 -o "${work}/${file}" "${base}/${path}"
  echo "${sha}  ${work}/${file}" | sha256sum -c --quiet - || { echo "Prüfsumme von ${file} falsch." >&2; exit 1; }
  mkdir -p "${work}/${file}.d"
  tar -xzf "${work}/${file}" -C "${work}/${file}.d"
  bundles="$(find "${work}/${file}.d" -type d -name '*.vst3' -prune)"
  [[ -n "$bundles" ]] || { echo "Kein VST3-Bundle in ${file}." >&2; exit 1; }
  while IFS= read -r bundle; do
    cp -a "$bundle" "${PLUGINS_OUT}/"
    echo "    $(basename "$bundle")"
  done <<<"$bundles"
done < <(grep -v '^\s*#' "$LOCK")

[[ "$found" == 1 ]] || echo "Hinweis: keine Linux-Plugins in ${LOCK}, Auslieferung ohne Lite-Plugins." >&2
