#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scan_only=false
if [[ "${1:-}" == "--scan-only" ]]; then
  scan_only=true
elif [[ $# -ne 0 ]]; then
  echo "usage: $0 [--scan-only]" >&2
  exit 2
fi

cd "$root"

if git ls-files | grep -Eq '(^|/)include/secrets\.h$'; then
  echo "error: a live secrets.h file is tracked" >&2
  exit 1
fi

if git grep -nE 'Serial\.(print|println|printf)\([^;]*WIFI_PASS' -- ':!scripts/verify.sh'; then
  echo "error: firmware still prints the Wi-Fi password" >&2
  exit 1
fi

python3 - "$root" <<'PY'
from pathlib import Path
import re
import subprocess
import sys

root = Path(sys.argv[1])
tracked = [p.decode() for p in subprocess.check_output(
    ["git", "-C", str(root), "ls-files", "-z"]
).split(b"\0") if p]

for rel in tracked:
    if rel.endswith("secrets.example.h"):
        continue
    path = root / rel
    try:
        text = path.read_text()
    except (UnicodeDecodeError, OSError):
        continue
    if re.search(r'^\s*#define\s+(WIFI_SSID|WIFI_PASS|OTA_PASSWORD)\s+"[^"]+"', text, re.M):
        raise SystemExit(f"error: credential-like literal remains in tracked source: {rel}")
PY

for example in \
  arduino-mega/include/secrets.example.h \
  esp32/include/secrets.example.h \
  esp32-advanced/include/secrets.example.h \
  esp32-full/include/secrets.example.h; do
  local_header="${example%.example.h}.h"
  git check-ignore -q "$local_header" || {
    echo "error: $local_header is not ignored" >&2
    exit 1
  }
done

echo "credential/configuration scan passed"

$scan_only && exit 0

pio_cmd=()
if command -v pio >/dev/null 2>&1; then
  pio_cmd=(pio)
elif command -v uvx >/dev/null 2>&1; then
  pio_cmd=(uvx --from platformio platformio)
else
  echo "error: PlatformIO CLI (pio) or uvx is required for firmware builds" >&2
  exit 1
fi

created=()
cleanup() {
  for file in "${created[@]}"; do
    rm -f "$file"
  done
}
trap cleanup EXIT

for project in arduino-mega esp32 esp32-advanced esp32-full; do
  secrets="$root/$project/include/secrets.h"
  if [[ ! -e "$secrets" ]]; then
    cp "$root/$project/include/secrets.example.h" "$secrets"
    created+=("$secrets")
  fi
  echo "building $project with local/example configuration"
  "${pio_cmd[@]}" run --project-dir "$root/$project"
done

echo "all firmware builds passed; no upload or device action was performed"
