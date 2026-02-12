#!/usr/bin/env python3
"""
Sync preset blocks between firmware and preset lab.

Modes:
  fw-to-lab : copy kSoundPresets block from SoundPresets.h into DEMO_BLOCK in index.html
  lab-to-fw : copy DEMO_BLOCK from index.html into SoundPresets.h and update PRESET_COUNT
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


FW_BLOCK_RE = re.compile(
    r"(static const SoundPreset kSoundPresets\[PRESET_COUNT\]\s*=\s*)(\{[\s\S]*?\n\};)"
)
LAB_BLOCK_RE = re.compile(r"(const DEMO_BLOCK = `)([\s\S]*?)(`;)")
PRESET_COUNT_RE = re.compile(r"(constexpr\s+uint8_t\s+PRESET_COUNT\s*=\s*)(\d+)(\s*;)")


def count_top_level_presets(block: str) -> int:
    depth = 0
    count = 0
    in_string = False
    escaping = False

    for ch in block:
        if in_string:
            if escaping:
                escaping = False
            elif ch == "\\":
                escaping = True
            elif ch == '"':
                in_string = False
            continue

        if ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
            if depth == 2:
                count += 1
        elif ch == "}":
            depth = max(0, depth - 1)

    return count


def replace_or_die(pattern: re.Pattern[str], text: str, replacement: str, what: str) -> str:
    updated, n = pattern.subn(replacement, text, count=1)
    if n != 1:
        raise RuntimeError(f"Could not locate {what}.")
    return updated


def sync_fw_to_lab(fw_path: Path, lab_path: Path) -> None:
    fw_text = fw_path.read_text(encoding="utf-8")
    lab_text = lab_path.read_text(encoding="utf-8")

    fw_match = FW_BLOCK_RE.search(fw_text)
    if not fw_match:
        raise RuntimeError("Could not find firmware kSoundPresets block.")
    fw_block = fw_match.group(2)

    lab_text = replace_or_die(
        LAB_BLOCK_RE,
        lab_text,
        rf"\1{fw_block}\3",
        "lab DEMO_BLOCK",
    )
    lab_path.write_text(lab_text, encoding="utf-8")

    print(f"Synced firmware -> lab ({count_top_level_presets(fw_block)} presets).")


def sync_lab_to_fw(fw_path: Path, lab_path: Path) -> None:
    fw_text = fw_path.read_text(encoding="utf-8")
    lab_text = lab_path.read_text(encoding="utf-8")

    lab_match = LAB_BLOCK_RE.search(lab_text)
    if not lab_match:
        raise RuntimeError("Could not find lab DEMO_BLOCK.")
    lab_block = lab_match.group(2)

    preset_count = count_top_level_presets(lab_block)
    if preset_count <= 0:
        raise RuntimeError("Lab DEMO_BLOCK appears to contain zero presets.")

    fw_text = replace_or_die(
        FW_BLOCK_RE,
        fw_text,
        rf"\1{lab_block}",
        "firmware kSoundPresets block",
    )
    fw_text = replace_or_die(
        PRESET_COUNT_RE,
        fw_text,
        rf"\g<1>{preset_count}\g<3>",
        "firmware PRESET_COUNT",
    )

    fw_path.write_text(fw_text, encoding="utf-8")
    print(f"Synced lab -> firmware ({preset_count} presets).")


def find_repo_root(start: Path) -> Path:
    cur = start.resolve()
    if cur.is_file():
        cur = cur.parent

    while True:
        if (cur / "AGENTS.md").is_file() and (cur / "projects").is_dir():
            return cur
        if cur.parent == cur:
            raise RuntimeError(
                "Could not infer repository root. Provide --repo-root, or explicit --fw-path and --lab-path."
            )
        cur = cur.parent


def main() -> int:
    parser = argparse.ArgumentParser(description="Sync Pico sound presets between firmware and lab.")
    parser.add_argument(
        "mode",
        choices=("fw-to-lab", "lab-to-fw"),
        help="Sync direction.",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        help="Repository root path (if omitted, inferred from this script location).",
    )
    parser.add_argument(
        "--fw-path",
        type=Path,
        help="Override firmware SoundPresets.h path.",
    )
    parser.add_argument(
        "--lab-path",
        type=Path,
        help="Override lab index.html path.",
    )
    args = parser.parse_args()

    root = args.repo_root.resolve() if args.repo_root else find_repo_root(Path(__file__))
    fw_path = args.fw_path.resolve() if args.fw_path else root / "projects" / "pico2w_lcd_soundboard" / "SoundPresets.h"
    lab_path = (
        args.lab_path.resolve()
        if args.lab_path
        else root / "projects" / "pico2w_lcd_soundboard" / "tools" / "preset_lab" / "index.html"
    )

    if not fw_path.is_file():
        raise RuntimeError(f"Missing firmware presets file: {fw_path}")
    if not lab_path.is_file():
        raise RuntimeError(f"Missing lab file: {lab_path}")

    if args.mode == "fw-to-lab":
        sync_fw_to_lab(fw_path, lab_path)
    else:
        sync_lab_to_fw(fw_path, lab_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
