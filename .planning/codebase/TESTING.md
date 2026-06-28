# Testing Patterns

**Analysis Date:** 2026-06-28

## Test Framework

**Runner:** None. This is an Arduino firmware project with no unit test framework configured.

There are no test files, no test directories, no `*.test.*` or `*.spec.*` files, and no testing library dependencies (Unity, Google Test, AUnit, etc.) in the project.

## Compilation as the Test

The sole automated correctness gate is successful compilation of the Arduino sketch. The CI pipeline (`arduino-cli compile`) treats a clean build as the passing condition.

**CI pipeline:** `.github/workflows/arduino-compile.yml`

**Trigger conditions:**
- Push to `main` or `snd` branches (path-filtered: `.md`, `docs/`, `images/` changes are ignored)
- Pull requests targeting `main`, `snd`, or `Dev` branches
- Manual `workflow_dispatch` with optional `create_release` toggle

**Runner:** Self-hosted (`git01`). The runner has Arduino CLI, ESP32 core, and library state persisted locally, so cache steps are skipped on that runner.

## Compile Step Details

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled \
  --output-dir ./build \
  ESP32-P4-Allsky-Display.ino 2>&1 | tee compile_output.txt
```

Key flags:
- Target: `esp32:esp32:esp32p4` (ESP32-P4 specific)
- Flash: 32MB with `app13M_data7M_32MB` partition scheme
- PSRAM: enabled (required for image buffers)
- `set -o pipefail` ensures non-zero `arduino-cli` exit code propagates through `tee`

The workflow fails the build step if `arduino-cli` exits non-zero, even with output piped through `tee`. This was an explicit fix (commit `a110ebb`).

## Pre-Compile Checks (Structural Lint)

The CI workflow runs three structural checks before compiling:

**1. Required files present** (`.github/workflows/arduino-compile.yml` lines 53-76):
```
README.md, .gitignore, ESP32-P4-Allsky-Display.ino,
config.h, config.cpp, config_storage.h, config_storage.cpp
```

**2. Header/source pair check** (lines 79-86):
Warns when a `.h` file has no matching `.cpp`. Exceptions: `web_config_html` (header-only PROGMEM asset file) and `displays_config`.

**3. File naming convention** (lines 89-105):
Validates all `.cpp`, `.h`, `.ino` files match `^[a-zA-Z0-9_-]+\.(cpp|h|ino)$`. Reports violations as warnings (does not fail the build).

## Library Installation in CI

The workflow installs and version-pins seven libraries (`arduino-compile.yml` lines 159-166):

| Library | Version |
|---------|---------|
| GFX Library for Arduino | 1.6.5 |
| JPEGDEC | 1.8.4 |
| PubSubClient | 2.8 |
| ElegantOTA | 3.1.7 |
| WebSockets | 2.7.2 |
| ArduinoJson | 7.4.3 |
| tgx | 1.1.1 |

The `tgx` library (moon sphere renderer) is installed the same way as all other libraries via `arduino-cli lib install tgx@1.1.1`. This was added in commit `a110ebb` after a previous workflow failed to install it.

**GFX Library patch:** After installation, a `sed` command patches `Arduino_ESP32DSIPanel.cpp` to replace `MIPI_DSI_PHY_CLK_SRC_DEFAULT` with `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M` for ESP32-P4 DSI compatibility (lines 172-176).

**ESP32 core version:** `esp32:esp32@3.3.7` (pinned, checked before installing to avoid redundant downloads on self-hosted runner).

## Post-Compile Checks

**Memory warning check** (lines 338-343): Scans `compile_output.txt` for patterns matching `warning.*memory` or `overflow`. Fails the build if found.

**Memory usage extraction** (lines 309-330): Parses flash and RAM usage from compile output. Results are reported in the CI log and, on `main` branch pushes, written to `.github/badges/flash-usage.json` and `.github/badges/ram-usage.json` on the `badges` branch for shield.io dynamic badge display.

## Release Artifacts

On push to `main` or `snd` (or manual trigger with `create_release=true`), the workflow produces:

- `ESP32-P4-Allsky-Display-OTA.bin`: application binary only, for OTA flashing via ElegantOTA at `http://[device]:8080/update`
- `ESP32-P4-Allsky-Display-Factory.bin`: merged binary (bootloader + partitions + app) for initial USB flash via `esptool.py` at address `0x0`

Merging uses `esptool.py --chip esp32p4 merge_bin` with offsets `0x2000` (bootloader), `0x8000` (partitions), `0x10000` (app).

Releases are created via `softprops/action-gh-release@v2`. Branch `snd` produces pre-release (test) builds tagged `v-snd-X.Y`. Branch `main` produces stable builds tagged `vX.Y`. Minor version auto-increments from the previous tag.

## PR Automation

Pull requests receive an AI-generated summary comment. The workflow calls Google Gemini 2.5 Flash (`gemini-2.5-flash:generateContent`) with the PR diff context. Workflow-only PRs (all changed files under `.github/workflows/`) skip the AI summary step.

## Manual Testing Approach

No documented manual test procedures exist in the repository. Practical verification is done by:

1. Flashing the Factory binary to a physical ESP32-P4 device via `esptool.py`
2. Observing boot behavior via Serial monitor (115200 baud)
3. Accessing the web configuration UI at `http://[device-ip]:8080`
4. Verifying image download, display rendering, MQTT connectivity, and OTA update flow manually on hardware

The live development device is accessible at `allskyesp3236.lan:8080`.

## Unit Test Gap

There are no unit tests for any of the following modules:
- `config_storage.cpp` (NVS read/write, dirty-field tracking, schema migration)
- `image_utils.cpp` (software image scaling)
- `web_config_api.cpp` (form parsing, config mutation)
- `command_interpreter.cpp` (serial command parsing)
- `moon_ephemeris.h` (ephemeris calculations)
- `config_backup.cpp` (JSON serialization, schema versioning)

All correctness verification for these modules depends on hardware-in-the-loop testing. There is no mock layer, no dependency injection, and no test harness infrastructure that would support off-target unit testing.

---

*Testing analysis: 2026-06-28*
