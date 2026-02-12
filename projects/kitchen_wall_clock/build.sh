#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PROJECT_DIR="$ROOT_DIR/projects/kitchen_wall_clock"
PROJECT="kitchen_wall_clock"
FQBN="rp2040:rp2040:rpipico2w"
BUILD_DIR="$ROOT_DIR/.build/$PROJECT"
NO_FLASH=0

# shellcheck source=scripts/helpers.sh
source "$ROOT_DIR/scripts/helpers.sh"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --no-flash)
      NO_FLASH=1
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--no-flash]"
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      exit 1
      ;;
  esac
done

if ! CLI="$(find_arduino_cli)"; then
  echo "arduino-cli not found in PATH or Arduino IDE default location." >&2
  exit 1
fi

echo "Building $PROJECT ($FQBN)"
"$CLI" compile \
  --fqbn "$FQBN" \
  --build-path "$BUILD_DIR" \
  --libraries "$ROOT_DIR/shared" \
  "$PROJECT_DIR"

UF2_PATH="$BUILD_DIR/$PROJECT.ino.uf2"
if [ ! -f "$UF2_PATH" ]; then
  echo "Expected UF2 artifact missing: $UF2_PATH" >&2
  exit 1
fi

echo "UF2:$UF2_PATH"
if [ "$NO_FLASH" -eq 0 ]; then
  "$ROOT_DIR/scripts/flash_uf2.sh" "$UF2_PATH"
else
  echo "FLASH:SKIPPED --no-flash"
fi
