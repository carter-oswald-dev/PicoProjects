#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if command -v arduino-cli >/dev/null 2>&1; then
  CLI="arduino-cli"
elif [[ -x "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" ]]; then
  CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
else
  echo "arduino-cli not found in PATH or Arduino IDE default location." >&2
  exit 1
fi

PROJECT="pico2w_lcd_soundboard"
FQBN="rp2040:rp2040:rpipico2w:ipbtstack=ipv4btcble"

echo "Building $PROJECT ($FQBN)"
"$CLI" compile \
  --fqbn "$FQBN" \
  --build-path "$ROOT_DIR/.build/$PROJECT" \
  "$ROOT_DIR/$PROJECT"
