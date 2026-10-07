# Build Environment Reference

Captured from a working machine for comparison/debugging.

## Dev Setup

```
py -3.13 -m venv .venv
.venv\Scripts\pip install -r requirements.txt
```

PlatformIO 6.1.19 is pinned in `requirements.txt`. Do not upgrade without testing a full build — the pioarduino platform is sensitive to the host PlatformIO version.

All PlatformIO commands use the venv: `.venv\Scripts\platformio run -e esp32_mcp2518fd`.
Or use `build_all_pio.bat` from the workspace root.

## Versions

- Python 3.13
- PlatformIO Core 6.1.19 (pinned in `.venv`)
- tool-scons 4.40801.0 (SCons 4.8.1) — bundled by pioarduino, do not upgrade independently

## Packages in `.pio/core/packages/`

```
contrib-piohome
framework-arduinoespressif32
framework-arduinoespressif32-libs
framework-espidf
tool-cmake
tool-esp-rom-elfs
tool-esptoolpy
tool-esp_install
tool-ninja
tool-scons
tool-xtensa-esp-elf-gdb
toolchain-xtensa-esp-elf
```

## CAN-FD Driver

`foodyfood/esp32-mcp2518fd-driver@1.1.3` is a normal PlatformIO registry dependency — no symlink.
PlatformIO downloads it automatically on first build into `.pio/libdeps/esp32_mcp2518fd/esp32-mcp2518fd-driver`.
Requires internet access on first build only.

## Build & Flash

Always use the `esp32_mcp2518fd` env — do not use `esp32devkit_330` (crashes on boot, GPIO 85 bug).

Flash command (from `ev-battery-bridge/` directory):
```
set PYTHONIOENCODING=utf-8 && .venv\Scripts\platformio run -e esp32_mcp2518fd --upload-port COM3 -t upload
```

First build is slow (~20 min) — framework compiles from source. Subsequent builds ~1-2 min.

## If the Port is Busy

```
tasklist
taskkill /PID <pid> /F
```

Kill any `esptool.exe` or stale `pio.exe` processes, then retry.

## Notes

- `Obsolete PIO Core v6.1.19` warnings during builds are harmless — a newer global PlatformIO is installed but the venv overrides it.
- The `ev-battery-bridge/.pio/core/penv` is owned entirely by pioarduino. Never manually install packages into it.
- If the penv gets corrupted (e.g. wrong PlatformIO version created it), delete `.pio/core/penv` and rebuild. The first build will recreate it cleanly in ~20 min.
- Unit tests (`ev-battery-simulator/tests/unit`) require a host GCC compiler. On Windows this means MinGW. They are not expected to pass without it.

Connect to WiFi AP `Battery-Emulator` / `123456789`, go to Settings and select:
- Battery = MEB
- Inverter = None
