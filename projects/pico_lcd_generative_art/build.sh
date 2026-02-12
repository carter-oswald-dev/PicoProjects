#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PROJECT_DIR="$ROOT_DIR/projects/pico_lcd_generative_art"
PROJECT="pico_lcd_generative_art"
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

if ! SDK_PATH="$(detect_pico_sdk_path)"; then
  echo "Could not resolve PICO_SDK_PATH. Set PICO_SDK_PATH or install Arduino RP2040 core (bundles pico-sdk)." >&2
  exit 1
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  if GCC_BIN="$(detect_arm_gcc_bin)"; then
    export PATH="$GCC_BIN:$PATH"
  fi
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "arm-none-eabi-gcc not found. Install the GNU Arm Embedded toolchain." >&2
  exit 1
fi

if ! PICOTOOL_BIN="$(detect_picotool_bin)"; then
  echo "picotool not found. Install picotool or Arduino RP2040 pqt-picotool package." >&2
  exit 1
fi

PICOTOOL_PKG_DIR="$ROOT_DIR/.build/picotool-cmake-package"
mkdir -p "$PICOTOOL_PKG_DIR"
cat > "$PICOTOOL_PKG_DIR/picotoolConfig.cmake" <<EOF
add_executable(picotool IMPORTED GLOBAL)
set_target_properties(picotool PROPERTIES IMPORTED_LOCATION "$PICOTOOL_BIN")
set(picotool_FOUND TRUE)
set(picotool_VERSION 2.1.1)
EOF
cat > "$PICOTOOL_PKG_DIR/picotoolConfigVersion.cmake" <<'EOF'
set(PACKAGE_VERSION "2.1.1")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_EXACT TRUE)
endif()
EOF

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
  -DPICO_PROJECTS_ROOT="$ROOT_DIR" \
  -DPICO_SDK_PATH="$SDK_PATH" \
  -Dpicotool_DIR="$PICOTOOL_PKG_DIR"

cmake --build "$BUILD_DIR"

UF2_PATH="$BUILD_DIR/GenerativeArt.uf2"
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
