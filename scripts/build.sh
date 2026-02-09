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

build_project() {
  local project="$1"
  local fqbn=""

  case "$project" in
    creepy_halloween_sonar)
      fqbn="rp2040:rp2040:rpipicow:ipbtstack=ipv4btcble"
      ;;
    pico2w_lcd_soundboard)
      fqbn="rp2040:rp2040:rpipico2w:ipbtstack=ipv4btcble"
      ;;
    *)
      echo "Unknown project: $project" >&2
      exit 1
      ;;
  esac

  echo "Building $project ($fqbn)"
  "$CLI" compile \
    --fqbn "$fqbn" \
    --build-path "$ROOT_DIR/.build/$project" \
    "$ROOT_DIR/$project"
}

usage() {
  cat <<EOF
Usage: $0 [all|creepy_halloween_sonar|pico2w_lcd_soundboard]
EOF
}

target="${1:-all}"

case "$target" in
  all)
    build_project "creepy_halloween_sonar"
    build_project "pico2w_lcd_soundboard"
    ;;
  creepy_halloween_sonar|pico2w_lcd_soundboard)
    build_project "$target"
    ;;
  *)
    usage
    exit 1
    ;;
esac
