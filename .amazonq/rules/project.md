# ev-battery-bridge Project Rules

## What this project is
Fork of dalathegreat/Battery-Emulator. Translates EV battery CAN-FD protocols to
inverter protocols for second-life storage. Our fork adds:
- `HW_ESP32_MCP2518FD` HAL — bare ESP32 DevKit V1 + MCP2518FD breakout
- `comm_can_mcp2518fd.cpp` — additive CAN driver using foodyfood/esp32-mcp2518fd-driver
- `esp32_mcp2518fd` PlatformIO env — our target env

Upstream repo: https://github.com/dalathegreat/Battery-Emulator
Our fork: https://github.com/HourPowerLimited/ev-battery-bridge (private)
Active branch: `devkit-canfd-interface`

---

## Code Review Tool
NEVER use the codeReview tool. Ever. For any reason.

---

## Additive-only principle
This fork must remain as close to upstream as possible for easy rebasing.
- NEVER modify files that exist in upstream unless absolutely unavoidable
- New functionality goes in new files
- `comm_can.cpp` is untouched — `comm_can_mcp2518fd.cpp` replaces it via `src_filter`
- `hal.cpp` has one added `#elif` block for `HW_ESP32_MCP2518FD` — minimal touch
- `Software.cpp` has one added define in the Serial init guard — minimal touch
- When rebasing from upstream: our new files have zero conflicts, only the minimal touches need review

---

## Hardware
- MCU: ESP32 DevKit V1 — CAN: MCP2518FD breakout — COM3
- SPI VSPI: SCK=33, MISO=35, MOSI=32, CS=25, INT=34
- CAN bus wired to ev-battery-simulator on COM4
- Both boards are identical hardware

---

## Flash Procedure
```
set PYTHONIOENCODING=utf-8 && pio run -e esp32_mcp2518fd --upload-port COM3 -t upload
```
- If port busy: `tasklist`, kill `esptool.exe` and stale `pio.exe` with `taskkill /PID <pid> /F`
- After flash: connect to WiFi `Battery-Emulator` / `123456789`, Settings → Battery = MEB, Inverter = None
- Serial capture: `python tools/serial_capture.py --port COM3 --secs 15`

---

## Build
- env: `esp32_mcp2518fd`
- Cache: `.pio/build_cache` — first build ~20 min, subsequent ~1-2 min
- Driver: `foodyfood/esp32-mcp2518fd-driver` v1.1.3 via local symlink `symlink://../../esp32-mcp2518fd-driver`
- When registry propagates v1.1.3, switch to `foodyfood/esp32-mcp2518fd-driver@1.1.3`
- clang-format pre-commit hook is installed — runs automatically on commit

---

## Current state
- MEB battery data flows end-to-end: simulator → CAN-FD bus → bridge → web UI
- SOC, cell voltages, charge/discharge limits, BMS mode all working
- Known gaps documented in `docs/specs/SPEC-001-meb-data-completeness.md`
- Next work: implement SPEC-001 gaps (see spec for implementation order)

---

## Key files
- `Software/src/communication/can/comm_can_mcp2518fd.cpp` — our CAN driver (additive)
- `Software/src/devboard/hal/hw_esp32_mcp2518fd.h` — our HAL (additive)
- `Software/src/battery/MEB-BATTERY.cpp` — upstream MEB driver (read carefully before touching)
- `platformio.ini` — `esp32_mcp2518fd` env at the top, `build_cache_dir` set
- `docs/specs/SPEC-001-meb-data-completeness.md` — gap analysis and acceptance criteria

---

## Do not use
- `esp32devkit_330` env — crashes on boot (GPIO 85 bug from ACAN2517FD + no LED)
- `pio device monitor` — use `python tools/serial_capture.py` instead
