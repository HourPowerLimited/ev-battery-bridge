# Build Environment Reference

Captured from a working machine for comparison/debugging.

## Versions

- Python 3.13.14
- PlatformIO Core 6.1.19
- tool-scons 4.40801.0 (SCons 4.8.1)

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

Flash command (from repo root):
```
set PYTHONIOENCODING=utf-8 && pio run -e esp32_mcp2518fd --upload-port COM3 -t upload
```

First build is slow (~20 min) — framework compiles from source. Subsequent builds ~1-2 min.

## If the Port is Busy

```
tasklist
taskkill /PID <pid> /F
```

Kill any `esptool.exe` or stale `pio.exe` processes, then retry.

## After Flashing

Connect to WiFi AP `Battery-Emulator` / `123456789`, go to Settings and select:
- Battery = MEB
- Inverter = None
