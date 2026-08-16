# SPEC-000 — ESP32 + MCP2518FD Hardware Bring-Up

## Status: Done

## What this covers
The work done to get a bare ESP32 DevKit V1 + MCP2518FD breakout running as a
CAN-FD bridge on top of the Battery-Emulator fork. This is a reference document,
not a forward spec.

---

## Hardware

| Item | Detail |
|---|---|
| MCU | ESP32 DevKit V1 |
| CAN controller | MCP2518FD breakout |
| Bus | SPI VSPI |
| SCK | GPIO 33 |
| MISO | GPIO 35 |
| MOSI | GPIO 32 |
| CS | GPIO 25 |
| INT | GPIO 34 |
| COM port | COM3 |
| CAN bus peer | ev-battery-simulator on COM4 (identical hardware) |

---

## Why a new env and HAL

Battery-Emulator upstream targets boards with onboard CAN transceivers. The
`esp32devkit_330` env uses ACAN2517FD and references GPIO 85, which does not exist
on a 38-pin DevKit — it crashes on boot. A new env and HAL were needed.

---

## What was added (additive-only)

### `hw_esp32_mcp2518fd.h`
Minimal HAL header. Defines only the MCP2518FD SPI pins; all other GPIOs set to
`GPIO_NUM_NC`. Added as a new `#elif HW_ESP32_MCP2518FD` block in `hal.cpp` —
the only upstream file touched beyond a single define.

### `comm_can_mcp2518fd.cpp`
Replaces `comm_can.cpp` for this env via `src_filter` in `platformio.ini`. Uses
`foodyfood/esp32-mcp2518fd-driver@1.1.3` from the PlatformIO registry. `comm_can.cpp` is untouched.

### `esp32_mcp2518fd` PlatformIO env
Defined at the top of `platformio.ini`. Key settings:
- `build_flags = -D HW_ESP32_MCP2518FD`
- `src_filter` excludes `comm_can.cpp`, includes `comm_can_mcp2518fd.cpp`
- `build_cache_dir = .pio/build_cache`

### `Software.cpp`
One added define inside the existing Serial init guard — minimal touch.

---

## Driver

`foodyfood/esp32-mcp2518fd-driver@1.1.3` from the PlatformIO registry.

---

## Flash procedure

```
set PYTHONIOENCODING=utf-8 && pio run -e esp32_mcp2518fd --upload-port COM3 -t upload
```

If upload fails with port busy: `tasklist`, kill `esptool.exe` and stale `pio.exe`
with `taskkill /PID <pid> /F`, then retry.

After flash: connect to WiFi `Battery-Emulator` / `123456789`, go to Settings,
select Battery = MEB, Inverter = None.

---

## Serial capture gotcha

Opening COM3 asserts DTR and resets the ESP32 — you see the boot log, not runtime
output. Use `python tools/serial_capture.py --port COM3 --secs 15`. A future fix
should pass `dsrdtr=False, rtscts=False` and set `dtr=False` after open.

---

## Build times

First build ~20 min (full upstream compile). Subsequent builds ~1–2 min (cache hit).

---

## End state

MEB battery data flows end-to-end: ev-battery-simulator → CAN-FD bus → bridge →
web UI. SOC, cell voltages, charge/discharge limits, and BMS mode all confirmed
working. Known data gaps are tracked in SPEC-003.
