# Retro-Go: Build, Flash & Play Guide (ESP32-P4-DevKit)

## 1. Prerequisites — Install ESP-IDF

You need **ESP-IDF v5.3+** (ESP32-P4 is only supported in v5.3+).

**Installation (Windows):**
- Download from: https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/get-started/windows-setup.html
- After install, use the **"ESP-IDF 5.3 CMD"** or **"ESP-IDF 5.3 PowerShell"** shortcut from Start Menu — this sets `IDF_PATH` and `PATH` correctly.

Then navigate to the project:
```powershell
cd D:\project\retro-go
```

---

## 2. Build Everything

### Full clean build + .img (recommended for first time):
```powershell
python rg_tool.py --target=esp32-p4-devkit --no-networking release
```

### Or build step-by-step:
```powershell
python rg_tool.py --target=esp32-p4-devkit --no-networking build      # Build all apps
python rg_tool.py --target=esp32-p4-devkit --no-networking build-img  # Pack into .img
```

### Build only specific apps (faster):
```powershell
python rg_tool.py --target=esp32-p4-devkit --no-networking build-img launcher prboom-go
```

Output: `retro-go_<version>_esp32-p4-devkit.img`

---

## 3. Flash the Full Image

Connect your ESP32-P4-DevKit via USB and find the COM port (check Device Manager → Ports). Then:

```powershell
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking install
```

---

## 4. Iterate Faster — Flash Individual Apps

After the full image is installed once:

```powershell
# Build, flash, and monitor an app in one go
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking run launcher

# Flash only a single updated app
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking flash retro-core

# Monitor serial output
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking monitor launcher
```

---

## 5. Adding Games to the SD Card

Format your SD card as **FAT32**. Insert it into your PC. The launcher scans specific folder names on the SD card root — each system has a fixed folder and accepted file extensions:

| System | SD Card Folder | File Extensions |
|--------|---------------|-----------------|
| NES | `SD:\nes\` | `.nes .fc .fds .nsf .zip` |
| SNES | `SD:\snes\` | `.smc .sfc .zip` |
| Game Boy | `SD:\gb\` | `.gb .gbc .zip` |
| Game Boy Color | `SD:\gbc\` | `.gbc .gb .zip` |
| Game Boy Advance | `SD:\gba\` | `.gba .zip` |
| Game & Watch | `SD:\gw\` | `.gw` |
| Master System | `SD:\sms\` | `.sms .sg .zip` |
| Game Gear | `SD:\gg\` | `.gg .zip` |
| Mega Drive / Genesis | `SD:\md\` | `.md .gen .bin .zip` |
| ColecoVision | `SD:\col\` | `.col .rom .zip` |
| PC Engine | `SD:\pce\` | `.pce .zip` |
| Atari Lynx | `SD:\lnx\` | `.lnx .zip` |
| DOOM | `SD:\doom\` | `.wad .zip` |
| MSX | `SD:\msx\` | `.rom .mx1 .mx2 .dsk` |

### How to copy games (Windows)

1. Insert SD card into your PC
2. Open File Explorer, find the SD card drive (e.g. `D:\`)
3. Create the matching folder if it doesn't exist (e.g. `D:\gb\`, `D:\pce\`)
4. Drag & drop your ROM files into the folder
5. Eject the SD card safely, insert into your device, and power on

Filenames can be anything — the launcher shows them in a list. ZIP files containing a single ROM are also supported.

### Complete SD card structure

```
SD:
  roms\
    gb\                   — Game Boy ROMs (.gb .gbc .zip)
      Pokemon.gb
      Zelda.gbc
    gbc\                  — Game Boy Color ROMs (.gbc .gb .zip)
    pce\                  — PC Engine ROMs (.pce .zip)
      R-Type.pce
    nes\                  — NES ROMs (.nes .fc .fds .nsf .zip)
    snes\                 — SNES ROMs (.smc .sfc .zip)
    gba\                  — GBA ROMs (.gba .zip)
    sms\                  — Master System ROMs (.sms .sg .zip)
    gg\                   — Game Gear ROMs (.gg .zip)
    md\                   — Mega Drive ROMs (.md .gen .bin .zip)
    col\                  — ColecoVision ROMs (.col .rom .zip)
    lnx\                  — Atari Lynx ROMs (.lnx .zip)
    gw\                   — Game & Watch (packed with LCD-Game-Shrinker)
    doom\                 — DOOM WADs (.wad .zip)
    msx\                  — MSX ROMs (.rom .mx1 .mx2 .dsk)
    romart\               — Cover art PNGs (optional)
      gb\
        Pokemon.png
      pce\
        R-Type.png
  retro-go\             — Config and BIOS (optional)
    bios\
      gb_bios.bin
      gbc_bios.bin
      fds_bios.bin
      msx\...
    config\
      wifi.json
```

---

## 6. Playing

1. Insert SD card, power on
2. **Launcher** opens — browse to your ROM folder
3. Select a game, press **A** to launch
4. Press **MENU** (GPIO 33) during gameplay for save states, filters, settings

---

## Quick Reference

| Command | Purpose |
|---------|---------|
| `python rg_tool.py --target=esp32-p4-devkit --no-networking release` | Clean build + pack .img |
| `python rg_tool.py --target=esp32-p4-devkit --no-networking build-img` | Build + pack .img |
| `python rg_tool.py --target=esp32-p4-devkit --no-networking install` | Flash full .img |
| `python rg_tool.py --target=esp32-p4-devkit --no-networking run <app>` | Build + flash + monitor |
| `python rg_tool.py --target=esp32-p4-devkit --no-networking flash <app>` | Flash single app |
| `python rg_tool.py --target=esp32-p4-devkit --no-networking monitor <app>` | Serial monitor |

**Apps:** `launcher`, `retro-core` (NES/PCE/G&W/Lynx/SMS/GG/COL), `prboom-go` (DOOM), `gwenesis` (Genesis), `fmsx` (MSX)
