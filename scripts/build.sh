#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<EOF
Usage: $0

Builds pico2w_lcd_soundboard only.
EOF
  exit 0
fi

exec "$ROOT_DIR/pico2w_lcd_soundboard/build.sh"
