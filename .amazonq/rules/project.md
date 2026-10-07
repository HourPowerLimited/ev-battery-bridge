# ev-battery-bridge

Fork of dalathegreat/Battery-Emulator. Translates EV battery CAN-FD protocols to inverter protocols for second-life storage.

Upstream: https://github.com/dalathegreat/Battery-Emulator  
Our fork: https://github.com/HourPowerLimited/ev-battery-bridge (private)  
Active branch: `devkit-canfd-interface`

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
- CAN bus wired to ev-battery-simulator on COM4

---

## Build
- env: `esp32_mcp2518fd` — always use this, never `esp32devkit_330`
- `esp32devkit_330` crashes on boot (GPIO 85 bug from ACAN2517FD + no LED)
- Driver: `foodyfood/esp32-mcp2518fd-driver@1.1.3`
- Cache: `.pio/build_cache` — first build ~20 min (IDF compiles from source), subsequent ~1-2 min
- clang-format pre-commit hook runs automatically on commit
- See `docs/build-environment-reference.md` for full environment details

---

## After flash
Join WiFi `Battery-Emulator` / `123456789` → Settings → Battery = MEB, Inverter = None  
Serial capture: `.venv\Scripts\python tools/serial_capture.py --port COM3 --secs 15`

---

## Current state
- MEB battery data flows end-to-end: simulator → CAN-FD bus → bridge → web UI
- SOC, cell voltages, charge/discharge limits, BMS mode all working
- Known gaps: `docs/specs/SPEC-001-meb-data-completeness.md`

---

## Key files
- `Software/src/communication/can/comm_can_mcp2518fd.cpp` — our CAN driver (additive)
- `Software/src/devboard/hal/hw_esp32_mcp2518fd.h` — our HAL (additive)
- `Software/src/battery/MEB-BATTERY.cpp` — upstream MEB driver (read carefully before touching)
- `platformio.ini` — `esp32_mcp2518fd` env at the top, `build_cache_dir` set
- `docs/specs/SPEC-001-meb-data-completeness.md` — gap analysis and acceptance criteria
