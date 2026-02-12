#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/helpers.sh
source "$ROOT_DIR/scripts/helpers.sh"

PROJECT=""
ALL=1
FAIL=0
SERIAL_CAPTURE_SECS=5

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

uf2_for_project() {
  case "$1" in
    creepy_halloween_sonar)
      printf '%s\n' "$ROOT_DIR/.build/creepy_halloween_sonar/creepy_halloween_sonar.ino.uf2"
      ;;
    pico2w_lcd_soundboard)
      printf '%s\n' "$ROOT_DIR/.build/pico2w_lcd_soundboard/pico2w_lcd_soundboard.ino.uf2"
      ;;
    pico_lcd_generative_art)
      printf '%s\n' "$ROOT_DIR/.build/pico_lcd_generative_art/GenerativeArt.uf2"
      ;;
    *)
      return 1
      ;;
  esac
}

capture_serial() {
  local port="$1"
  local output_file="$2"

  if stty -f "$port" 115200 raw -echo >/dev/null 2>&1; then
    :
  elif stty -F "$port" 115200 raw -echo >/dev/null 2>&1; then
    :
  else
    return 1
  fi

  cat "$port" > "$output_file" &
  local cat_pid=$!
  sleep "$SERIAL_CAPTURE_SECS"
  kill "$cat_pid" >/dev/null 2>&1 || true
  wait "$cat_pid" >/dev/null 2>&1 || true
  return 0
}

run_smoke_for_project() {
  local project="$1"
  local uf2
  uf2="$(uf2_for_project "$project")"

  if [ ! -f "$uf2" ]; then
    echo "SMOKE:$project:FAIL missing uf2 artifact at $uf2" >&2
    FAIL=1
    return
  fi

  if ! MOUNT_PATH="$(detect_boot_mount)"; then
    echo "SMOKE:$project:SKIPPED no RPI-RP2 boot mount detected"
    return
  fi

  local before_ports
  before_ports="$(list_usb_serial_ports || true)"

  if ! "$ROOT_DIR/scripts/flash_uf2.sh" "$uf2"; then
    echo "SMOKE:$project:FAIL flash step failed" >&2
    FAIL=1
    return
  fi

  sleep 2

  local after_ports
  after_ports="$(list_usb_serial_ports || true)"
  if [ -z "$after_ports" ]; then
    echo "SMOKE:$project:SKIPPED flashed, but no USB serial port detected"
    return
  fi

  if [ "$project" = "pico_lcd_generative_art" ]; then
    local port
    port="$(printf '%s\n' "$after_ports" | head -n 1)"
    local tmp
    tmp="$(mktemp)"
    if capture_serial "$port" "$tmp"; then
      if grep -Eq "Frame time:|LCD write time:" "$tmp"; then
        echo "SMOKE:$project:PASS flashed + serial signature detected on $port"
      else
        echo "SMOKE:$project:FAIL flashed but expected serial signature not found on $port" >&2
        FAIL=1
      fi
    else
      echo "SMOKE:$project:SKIPPED flashed, but could not capture serial from $port"
    fi
    rm -f "$tmp"
  else
    if [ -n "$before_ports" ] || [ -n "$after_ports" ]; then
      echo "SMOKE:$project:PASS flashed + USB serial enumerated"
    else
      echo "SMOKE:$project:SKIPPED flashed, serial enumeration unavailable"
    fi
  fi
}

if [ "$ALL" -eq 1 ]; then
  run_smoke_for_project creepy_halloween_sonar
  run_smoke_for_project pico2w_lcd_soundboard
  run_smoke_for_project pico_lcd_generative_art
else
  case "$PROJECT" in
    creepy_halloween_sonar|pico2w_lcd_soundboard|pico_lcd_generative_art)
      run_smoke_for_project "$PROJECT"
      ;;
    *)
      echo "unknown project: $PROJECT" >&2
      exit 1
      ;;
  esac
fi

if [ "$FAIL" -ne 0 ]; then
  exit 1
fi
