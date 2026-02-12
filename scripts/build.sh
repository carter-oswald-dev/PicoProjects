#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PROJECT=""
NO_FLASH=0
ALL=1

usage() {
  cat <<USAGE
Usage: $0 [--all] [--project <name>] [--no-flash]

Projects:
  creepy_halloween_sonar
  pico2w_lcd_soundboard
  pico_lcd_generative_art
  kitchen_wall_clock
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
    --no-flash)
      NO_FLASH=1
      shift
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

build_one() {
  local p="$1"
  local cmd=("$ROOT_DIR/projects/$p/build.sh")
  if [ "$NO_FLASH" -eq 1 ]; then
    cmd+=("--no-flash")
  fi
  echo "BUILD:START $p"
  "${cmd[@]}"
  echo "BUILD:PASS $p"
}

if [ "$ALL" -eq 1 ]; then
  build_one "creepy_halloween_sonar"
  build_one "pico2w_lcd_soundboard"
  build_one "pico_lcd_generative_art"
  build_one "kitchen_wall_clock"
else
  case "$PROJECT" in
    creepy_halloween_sonar|pico2w_lcd_soundboard|pico_lcd_generative_art|kitchen_wall_clock)
      build_one "$PROJECT"
      ;;
    *)
      echo "unknown project: $PROJECT" >&2
      exit 1
      ;;
  esac
fi
