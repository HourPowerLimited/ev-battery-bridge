# SPEC-001 — Bridge Serial API

## Status
Pending

## Background

The bridge's datalayer is only observable via the web UI `/advanced` page, which
requires WiFi and a browser. There is no machine-readable interface over the USB serial
port. This blocks automated integration testing entirely.

This spec adds a minimal read-only serial API to the bridge so that automated tests
(and any other tooling) can query decoded datalayer values over USB serial without
human interaction.

---

## Design

### Activation
The API is **always active** unless `usb_logging_active` is true. Logging and the API
are mutually exclusive — they share the same physical USB serial port. When logging is
off (the default), the port is in API mode. When logging is on, the port streams log
output and the API is silently disabled. No UI toggle, no NVM setting.

### Transport
- USB serial, 115200 baud
- Line-oriented: `\n`-terminated commands, `\n`-terminated responses

### Commands

```
GET <key>    ->  <key>=<value>\n    on success
             ->  ERR unknown_key\n  on unknown key
```

No `SET` — the bridge is a CAN receiver. Its state comes from the CAN bus, not the
host. Writing datalayer values from serial would produce a false test signal.

### Keys (initial set)

| Key | Source | Type |
|---|---|---|
| `soc` | `datalayer.battery.status.real_soc` | uint16, units 0.01% |
| `voltage_dV` | `datalayer.battery.status.voltage_dV` | uint16, dV |
| `cell_min_mV` | `datalayer.battery.status.cell_min_voltage_mV` | uint16, mV |
| `cell_max_mV` | `datalayer.battery.status.cell_max_voltage_mV` | uint16, mV |
| `max_charge_W` | `datalayer.battery.status.max_charge_power_W` | uint32, W |
| `max_discharge_W` | `datalayer.battery.status.max_discharge_power_W` | uint32, W |
| `bms_mode` | `datalayer_extended.meb.BMS_mode` | uint8 |
| `rt_overcurrent` | `datalayer_extended.meb.rt_overcurrent` | uint8, 0-3 |
| `rt_cell_overvolt` | `datalayer_extended.meb.rt_cell_overvolt` | uint8, 0-3 |
| `rt_battery_unathorized` | `datalayer_extended.meb.rt_battery_unathorized` | uint8, 0-3 |
| `can_alive` | `datalayer.battery.status.CAN_battery_still_alive` | uint8 |

Keys are lowercase, underscore-separated. Adding a key is a one-line change to the
lookup table.

---

## Implementation

New files: `Software/src/devboard/serial/serial_api.h` and `serial_api.cpp`

- `serial_api_tick()` called from the main loop
- Returns immediately if `datalayer.system.info.usb_logging_active` is true
- Reads lines from `Serial`, parses `GET <key>`
- Looks up key in a static `{name, getter}` table where getter is `uint32_t(*)()`
- Prints `<key>=<value>\n`
- Stack-local line buffer, no heap allocation, no RTOS primitives

`Software/src/Software.cpp` — add `serial_api_tick()` call in the main loop.

---

## Acceptance criteria

- **AC-1:** `GET soc` returns `soc=<digits>` within 2s when `usb_logging_active` is
  false (the default).
- **AC-2:** `GET unknown` returns `ERR unknown_key`.
- **AC-3:** All keys in the table return a correctly formatted `key=value` response.
- **AC-4:** When `usb_logging_active` is true, `GET soc` receives no response within
  2s (API disabled).

---

## Files affected

- `Software/src/devboard/serial/serial_api.h` — new
- `Software/src/devboard/serial/serial_api.cpp` — new
- `Software/src/Software.cpp` — add `serial_api_tick()` call
