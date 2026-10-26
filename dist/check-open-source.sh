#!/usr/bin/env bash
# Prüft, dass in diesem Repo nur liegt, was die DAW selbst braucht (ADR 0008): Code und Doku, die nur
# eigenständige Plugins nutzen, gehören nicht hierher. Läuft in der CI und vor jedem Release.
#
# Aufruf:  dist/check-open-source.sh
# Prüft die Dateien im Git-Index und neue, nicht ignorierte Dateien. Exit 1 bei einem Fund.
set -euo pipefail
cd "$(dirname "$0")/.."

found=0
fail() { echo "GEHÖRT NICHT IN DIESES REPO: $1" >&2; found=1; }

files="$(git ls-files --cached --others --exclude-standard | grep -v '^external/' || true)"

# Ordner, die es hier bewusst nicht gibt: Produkt-Entwürfe, Lern- und Messdaten der Instrumente.
while IFS= read -r pattern; do
  [[ -z "$pattern" ]] && continue
  if grep -E -q "$pattern" <<<"$files"; then
    fail "$(grep -E "$pattern" <<<"$files" | head -3 | tr '\n' ' ')(Muster $pattern)"
  fi
done <<'PATTERNS'
^docs/products/
^lab/
^src/products/
PATTERNS

# Einstellungen mit Zugangsdaten werden nie eingecheckt.
if grep -E -q '(^|/)dist\.env$' <<<"$files"; then fail "dist.env (Einstellungen gehören nicht ins Git)"; fi

if [[ "$found" == 1 ]]; then
  echo "==> Bitte entfernen (ADR 0008)." >&2
  exit 1
fi
echo "==> Repo enthält nur die DAW."
