#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PROJECT=""
ALL=1

usage() {
  cat <<USAGE
Usage: $0 [--all] [--project <name>]
USAGE
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --all)
      ALL=1
      PROJECT=""
      shift
      ;;
    --project)
      [ "$#" -ge 2 ] || { echo "missing value for --project" >&2; exit 1; }
      ALL=0
      PROJECT="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

"$ROOT_DIR/scripts/doctor.sh"

if [ "$ALL" -eq 1 ]; then
  "$ROOT_DIR/scripts/build.sh" --all --no-flash
else
  "$ROOT_DIR/scripts/build.sh" --project "$PROJECT" --no-flash
fi

# Preset sync regression for the soundboard toolchain path.
if [ "$ALL" -eq 1 ] || [ "$PROJECT" = "pico2w_lcd_soundboard" ]; then
  FW_PATH="$ROOT_DIR/projects/pico2w_lcd_soundboard/SoundPresets.h"
  LAB_PATH="$ROOT_DIR/projects/pico2w_lcd_soundboard/tools/preset_lab/index.html"

  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TMP_DIR"' EXIT

  cp "$FW_PATH" "$TMP_DIR/SoundPresets.before.h"
  cp "$LAB_PATH" "$TMP_DIR/index.before.html"
  cp "$FW_PATH" "$TMP_DIR/SoundPresets.work.h"
  cp "$LAB_PATH" "$TMP_DIR/index.work.html"

  python3 "$ROOT_DIR/projects/pico2w_lcd_soundboard/tools/preset_lab/sync_presets.py" \
    fw-to-lab \
    --fw-path "$TMP_DIR/SoundPresets.work.h" \
    --lab-path "$TMP_DIR/index.work.html"

  python3 "$ROOT_DIR/projects/pico2w_lcd_soundboard/tools/preset_lab/sync_presets.py" \
    lab-to-fw \
    --fw-path "$TMP_DIR/SoundPresets.work.h" \
    --lab-path "$TMP_DIR/index.work.html"

  if cmp -s "$TMP_DIR/SoundPresets.before.h" "$TMP_DIR/SoundPresets.work.h"; then
    echo "VERIFY:PASS preset sync roundtrip stable"
  else
    echo "VERIFY:FAIL preset sync roundtrip modified firmware content" >&2
    exit 1
  fi
fi

if [ "$ALL" -eq 1 ]; then
  "$ROOT_DIR/scripts/smoke.sh" --all
else
  "$ROOT_DIR/scripts/smoke.sh" --project "$PROJECT"
fi

echo "VERIFY:RESULT PASS"
