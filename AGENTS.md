# AGENTS.md — PicoProjects (Codex / coding agents)

This repository contains multiple Raspberry Pi Pico W / Pico 2W projects.
Some projects build with **arduino-cli**, others build with the **Raspberry Pi Pico C SDK** (CMake).
There is a shared library folder under `/shared` that must remain buildable under **both toolchains**.

## Repo layout (high level)

- `/projects/` — individual projects (each has its own build script)
- `/shared/` — shared libraries used by multiple projects (LCD driver, text rendering, etc.)
- `/scripts/` — build/helper scripts; must work on **Linux and macOS**

## Key rules (must follow)

### 1) Shared code compatibility
- Code under `/shared` **must compile under both**:
  - Pico C SDK toolchain (CMake)
  - Arduino toolchain (arduino-cli)
- Avoid assumptions that only one toolchain supports:
  - Keep headers C-friendly (`extern "C"` guards if needed)
  - Avoid non-portable compiler flags in shared headers
  - Keep dependencies minimal and explicit

### 2) Change impact / verification policy
- If **any file under `/shared` changes**:
  - You must at least **verify successful builds of all projects** that depend on it.
  - If feasible in time, run all project build scripts under `/projects`.
- If only a single project changes (not shared):
  - Build and verify that project.

### 3) Always build the active project
For every requested change:
- Identify the **active project** (the one being modified / discussed).
- Build it using its build script under `/projects/<project>/...`.
- If a Pico board is mounted, flash the produced firmware.

> “Active project” = the project you are editing in this session.
> If unclear, infer it from what files are being modified (but do not ask unless truly ambiguous).

### 4) Flashing policy (only if mounted)
After a successful build, if a board is connected in USB mass storage boot mode:
- Detect typical mount points:
  - macOS: `/Volumes/RPI-RP2`
  - Linux: `/media/$USER/RPI-RP2` or `/run/media/$USER/RPI-RP2`
- Copy the `.uf2` to the mounted drive.
- If not mounted, do not attempt flashing—just report build success and artifact path.

## Build workflow expectations

### Project build scripts are the source of truth
Each project under `/projects` has a build script that chooses either:
- `arduino-cli` build (and possibly upload), OR
- Pico SDK `cmake` + `ninja/make` build

Use those scripts instead of inventing new build steps.

### Scripts must be cross-platform (Linux + macOS)
- `/scripts` must run on Linux and macOS
- Prefer POSIX shell. Avoid GNU-only flags where easy (or detect OS and branch).
- When using `sed`, prefer portable patterns or handle BSD vs GNU differences.
- Prefer python for non-trivial portability needs.

## Coding conventions (shared libraries)

### API & layering
Public headers should be stable and minimal.

### Memory & allocation
- Prefer static allocation.
- Avoid heap allocation in core hot-path code.

### Includes
- Shared headers should not assume Arduino or Pico SDK include paths implicitly.
- Keep `#include` dependencies minimal; include what you use.

## What to run before finishing

At minimum (every change):
1) Build the active project via its build script.
2) If `/shared` changed: build/verify dependent projects (ideally all).
3) If Pico is mounted: flash `.uf2`.

When you report back:
- State which projects were built/verified.
- State whether flashing occurred and to which mount path.
- If something couldn’t be built due to missing toolchain locally, say so explicitly and list what’s needed.

## Notes for agents
- Keep refactors incremental and avoid “big bang” reorganizations unless asked.
- Preserve existing behavior. Don’t add new features unless requested.
- Prefer small reusable libraries in `/shared` that both toolchains can consume.
