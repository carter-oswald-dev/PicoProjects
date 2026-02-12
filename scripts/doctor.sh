#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/helpers.sh
source "$ROOT_DIR/scripts/helpers.sh"

FAIL=0

pass() { echo "DOCTOR:PASS $1"; }
warn() { echo "DOCTOR:FAIL $1" >&2; FAIL=1; }

if CLI="$(find_arduino_cli)"; then
  pass "arduino-cli found at $CLI"
else
  warn "arduino-cli not found. Install arduino-cli or Arduino IDE."
fi

if command -v arduino-cli >/dev/null 2>&1; then
  if arduino-cli core list | grep -Eq '^rp2040:rp2040([[:space:]]|$)'; then
    pass "Arduino core rp2040:rp2040 installed"
  else
    warn "Arduino core rp2040:rp2040 missing. Run: arduino-cli core install rp2040:rp2040"
  fi
fi

if command -v cmake >/dev/null 2>&1; then
  pass "cmake found: $(cmake --version | head -n 1)"
else
  warn "cmake missing. Install CMake 3.13+"
fi

if command -v python3 >/dev/null 2>&1; then
  pass "python3 found"
else
  warn "python3 missing"
fi

if SDK_PATH="$(detect_pico_sdk_path)"; then
  pass "Pico SDK path resolved: $SDK_PATH"
else
  warn "Pico SDK path not found. Set PICO_SDK_PATH or install Arduino RP2040 core that bundles pico-sdk."
fi

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  pass "arm-none-eabi-gcc found in PATH"
else
  if GCC_BIN="$(detect_arm_gcc_bin)"; then
    pass "arm-none-eabi-gcc available at $GCC_BIN (will be added by build scripts)"
  else
    warn "arm-none-eabi-gcc not found. Install GCC toolchain or Arduino RP2040 core tools."
  fi
fi

if PICOTOOL_BIN="$(detect_picotool_bin)"; then
  pass "picotool found at $PICOTOOL_BIN"
else
  warn "picotool not found. Install picotool (or Arduino RP2040 pqt-picotool) to avoid Pico SDK fetch-from-git."
fi

if [ "$FAIL" -ne 0 ]; then
  echo "DOCTOR:RESULT FAIL" >&2
  exit 1
fi

echo "DOCTOR:RESULT PASS"
