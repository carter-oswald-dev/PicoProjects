#!/usr/bin/env bash

find_arduino_cli() {
  if command -v arduino-cli >/dev/null 2>&1; then
    command -v arduino-cli
    return 0
  fi

  local mac_cli="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
  if [ -x "$mac_cli" ]; then
    printf '%s\n' "$mac_cli"
    return 0
  fi

  return 1
}

detect_pico_sdk_path() {
  if [ -n "${PICO_SDK_PATH:-}" ] && [ -d "${PICO_SDK_PATH}" ]; then
    printf '%s\n' "${PICO_SDK_PATH}"
    return 0
  fi

  local base
  local latest
  local candidate

  for base in \
    "$HOME/Library/Arduino15/packages/rp2040/hardware/rp2040" \
    "$HOME/.arduino15/packages/rp2040/hardware/rp2040"
  do
    [ -d "$base" ] || continue
    latest=""
    for candidate in "$base"/*/pico-sdk; do
      [ -d "$candidate" ] || continue
      latest="$candidate"
    done
    if [ -n "$latest" ] && [ -d "$latest" ]; then
      printf '%s\n' "$latest"
      return 0
    fi
  done

  for base in \
    "$HOME/.pico-sdk/sdk" \
    "$HOME/pico-sdk"
  do
    [ -d "$base" ] || continue
    if [ -f "$base/pico_sdk_init.cmake" ]; then
      printf '%s\n' "$base"
      return 0
    fi

    latest=""
    for candidate in "$base"/*; do
      [ -d "$candidate" ] || continue
      if [ -f "$candidate/pico_sdk_init.cmake" ]; then
        latest="$candidate"
      fi
    done
    if [ -n "$latest" ] && [ -f "$latest/pico_sdk_init.cmake" ]; then
      printf '%s\n' "$latest"
      return 0
    fi
  done

  return 1
}

detect_arm_gcc_bin() {
  if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    dirname "$(command -v arm-none-eabi-gcc)"
    return 0
  fi

  local base
  local latest
  local candidate

  for base in \
    "$HOME/.pico-sdk/toolchain" \
    "$HOME/Library/Arduino15/packages/rp2040/tools/pqt-gcc" \
    "$HOME/.arduino15/packages/rp2040/tools/pqt-gcc"
  do
    [ -d "$base" ] || continue
    latest=""
    for candidate in "$base"/*/bin; do
      [ -x "$candidate/arm-none-eabi-gcc" ] || continue
      latest="$candidate"
    done
    if [ -n "$latest" ] && [ -x "$latest/arm-none-eabi-gcc" ]; then
      printf '%s\n' "$latest"
      return 0
    fi
  done

  return 1
}

detect_picotool_bin() {
  if command -v picotool >/dev/null 2>&1; then
    command -v picotool
    return 0
  fi

  local base
  local latest
  local candidate

  for base in \
    "$HOME/.pico-sdk/picotool" \
    "$HOME/Library/Arduino15/packages/rp2040/tools/pqt-picotool" \
    "$HOME/.arduino15/packages/rp2040/tools/pqt-picotool"
  do
    [ -d "$base" ] || continue
    latest=""
    for candidate in "$base"/*/picotool "$base"/*/picotool/picotool; do
      [ -x "$candidate" ] || continue
      latest="$candidate"
    done
    if [ -n "$latest" ] && [ -x "$latest" ]; then
      printf '%s\n' "$latest"
      return 0
    fi
  done

  return 1
}

detect_boot_mount() {
  local user_name="${USER:-$(id -un)}"
  local mount
  for mount in \
    "/Volumes/RPI-RP2" \
    "/Volumes/RP2350" \
    "/media/${user_name}/RPI-RP2" \
    "/media/${user_name}/RP2350" \
    "/run/media/${user_name}/RPI-RP2" \
    "/run/media/${user_name}/RP2350"
  do
    if [ -d "$mount" ]; then
      printf '%s\n' "$mount"
      return 0
    fi
  done
  return 1
}

list_usb_serial_ports() {
  local candidate
  for candidate in \
    /dev/cu.usbmodem* \
    /dev/cu.usbserial* \
    /dev/ttyACM* \
    /dev/ttyUSB*
  do
    [ -e "$candidate" ] || continue
    printf '%s\n' "$candidate"
  done | sort -u
}
