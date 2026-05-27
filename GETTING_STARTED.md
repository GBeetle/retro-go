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

## 5. SD Card Setup

Insert an SD card with this structure:

```
SD:\
  nes\            -> Super Mario Bros.nes
  gb\             -> Pokemon.gb
  gbc\            -> Zelda.gbc
  sega\           -> Sonic.sms
  sg1000\         -> 
  pce\            -> R-Type.pce
  lynx\           ->
  coleco\         ->
  genesis\        -> Sonic.md  (or megadrive\)
  snes\           ->
  doom\           -> doom1.wad
  msx\            ->
  gw\             -> (Game & Watch, packed with LCD-Game-Shrinker)
  romart\         -> (cover art, optional)
  retro-go\
    bios\         -> BIOS files (optional)
    config\       -> wifi.json (optional)
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
