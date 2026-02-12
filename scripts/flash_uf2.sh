#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/helpers.sh
source "$ROOT_DIR/scripts/helpers.sh"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ "$#" -ne 1 ]; then
  cat <<USAGE
Usage: $0 <firmware.uf2>
USAGE
  exit 0
fi

UF2_PATH="$1"
if [ ! -f "$UF2_PATH" ]; then
  echo "FLASH:FAIL uf2 file not found: $UF2_PATH" >&2
  exit 1
fi

if ! MOUNT_PATH="$(detect_boot_mount)"; then
  echo "FLASH:SKIPPED no RPI-RP2/RP2350 mount found"
  exit 0
fi

cp "$UF2_PATH" "$MOUNT_PATH/"
echo "FLASH:PASS copied $UF2_PATH -> $MOUNT_PATH/"
