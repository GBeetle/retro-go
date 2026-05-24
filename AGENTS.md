# Retro-Go - Agent Guide

## Build system

- **Always use `python rg_tool.py`**, never `idf.py` directly. `rg_tool.py` wraps `idf.py` and handles multi-app image building, partition tables, and device-specific env.
- Default target is `odroid-go`; override via `--target <name>` or `RG_TOOL_TARGET` env var.
- Default serial port is `COM3`; override via `--port <port>` or `RG_TOOL_PORT` env var.
- See all commands: `python rg_tool.py --help`.
- Requires ESP-IDF 4.4-5.3 installed with `IDF_PATH` set.
- On Windows, always prefix with `python` (not `./rg_tool.py`).

## Key commands

- `python rg_tool.py release` - Clean build + pack .fw + .img for default target
- `python rg_tool.py build-fw [app...]` - Build + pack .fw
- `python rg_tool.py build-img [app...]` - Build + pack .img (serial flash)
- `python rg_tool.py build [app...]` - Build only
- `python rg_tool.py clean [app...]` - Remove build artifacts
- `python rg_tool.py flash <app>` - Flash single app to device
- `python rg_tool.py monitor <app>` - Serial monitor for app
- `python rg_tool.py run <app>` - flash + monitor in sequence
- `python rg_tool.py install` - Flash full .img to device
- `python rg_tool.py --target=<name> --port=<port> run <app>` - Build, flash, monitor a single app for fast iteration

## Architecture

- **Multi-app firmware**: devices run a launcher + several emulator apps as independent ESP-IDF projects.
- Five app slots: `launcher`, `retro-core`, `prboom-go`, `gwenesis`, `fmsx`.
- `retro-core` bundles NES, PCE, G&W, Lynx, SMS/GG/COL (cannot be built individually).
- Targets (devices) in `components/retro-go/targets/<name>/` with config.h, env.py, sdkconfig.
- Shared framework in `components/retro-go/` (display, audio, input, GUI, storage, network, settings).
- Tested IDF versions: 4.4.8 (primary), 4.3 (1.35-1.43), 4.1 (1.20-1.34).

## Porting a new device

- Clone an existing target folder in `components/retro-go/targets/`.
- Add the target to `components/retro-go/config.h` include chain.
- Edit config.h (pins, display, input), env.py (chip type, baud), sdkconfig (esp-idf config).
- Build with: `python rg_tool.py --target=<name> build launcher` first, then full.
- Need PSRAM. CPU freq at max, power management disabled. FATFS LFN + UTF-8 enabled.
- ESP-IDF patches for SD card fix and panic hook in `tools/patches/`.

## Code conventions

- C11 / C++11 standards. Compiler: xtensa-esp32-elf-gcc.
- Strings for i18n are wrapped in `_(...)`; translations in `translations.h`.
- Images for launcher in `themes/default/`; edit them, then run `tools/gen_images.py` to regenerate `launcher/main/images.c`.
- Screen driver: ILI9341/ST7789 by default. Custom drivers added via RG_SCREEN_DRIVER in rg_display.c.
- Input drivers: GPIO, ADC, I2C, shift register, virtual. Combined via RG_GAMEPAD_*_MAP.
- Theme system: JSON + PNG in `sd:/retro-go/themes/`.

## CI

- Builds with `espressif/idf:release-v4.4` Docker image.
- Applies `panic-hook` and `sdcard-fix` patches from `tools/patches/`.
- Produces .fw artifacts for odroid-go and mrgc-g32 targets.
- No automated tests (embedded firmware).

## Debugging

- Crash logs saved to `/sd/crash.log` if panic-hook patch is applied.
- Resolve backtraces: `xtensa-esp32-elf-addr2line -ifCe app-name/build/app-name.elf`.
- Hold DOWN on power-up to force recovery to launcher.
- Recovery button configurable via RG_RECOVERY_BTN in target config.h.
