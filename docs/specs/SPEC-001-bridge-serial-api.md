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
GET <key>    ->  <key>=<value>\n    key exists and has a value
             ->  ERR unknown_key\n  key not registered (unknown, or battery not loaded)
```

No `SET` — the bridge is a CAN receiver. Its state comes from the CAN bus, not the
host. Writing datalayer values from serial would produce a false test signal.

Battery-specific keys (e.g. MEB `rt_*` fields) are only registered when that battery
is the active battery. If a different battery is loaded those keys simply do not exist
and return `ERR unknown_key`. The test harness discovers available keys by attempting
`GET` and checking for the error response.

---

## Key table

All values are integers. Scaling and units are documented per key.

### Core battery status (`datalayer.battery.status`)

| Key | Source field | Unit / scaling | Notes |
|---|---|---|---|
| `soc` | `real_soc` | 0.01% (8000 = 80.00%) | Raw BMS SOC |
| `soh` | `soh_pptt` | 0.01% (9900 = 99.00%) | State of health |
| `voltage_dV` | `voltage_dV` | dV (3700 = 370.0 V) | Pack voltage |
| `current_dA` | `current_dA` | dA, signed (95 = 9.5 A) | Pack current, positive = charging |
| `power_W` | `active_power_W` | W, signed | Instantaneous power |
| `max_charge_W` | `max_charge_power_W` | W | BMS charge power limit |
| `max_discharge_W` | `max_discharge_power_W` | W | BMS discharge power limit |
| `remaining_Wh` | `remaining_capacity_Wh` | Wh | Usable energy remaining |
| `cell_min_mV` | `cell_min_voltage_mV` | mV | Lowest cell voltage |
| `cell_max_mV` | `cell_max_voltage_mV` | mV | Highest cell voltage |
| `temp_min_dC` | `temperature_min_dC` | 0.1°C, signed (150 = 15.0°C) | Minimum pack temperature |
| `temp_max_dC` | `temperature_max_dC` | 0.1°C, signed | Maximum pack temperature |
| `can_alive` | `CAN_battery_still_alive` | countdown (0 = lost) | Non-zero = CAN frames arriving |
| `bms_status` | `real_bms_status` | enum: 0=DISCONNECTED 1=STANDBY 2=ACTIVE 3=FAULT | |
| `system_status` | `datalayer.system.status.system_status` | enum: 0=STANDBY 1=INACTIVE 3=ACTIVE 4=FAULT 5=UPDATING | |

### MEB extended — BMS state (`datalayer_extended.meb`)

| Key | Source field | Unit / scaling | Notes |
|---|---|---|---|
| `bms_mode` | `BMS_mode` | enum: 0=HV_inactive 1=HV_active 2=Balancing 3=Extern_charging 4=AC_charging 5=Error 6=DC_charging 7=Init | |
| `hvil` | `HVIL` | enum: 0=Init 1=Closed 2=Open 3=Fault | |
| `isolation_kOhm` | `isolation_resistance` | kΩ | |
| `voltage_intermediate_dV` | `BMS_voltage_intermediate_dV` | dV, signed | Pre-contactor voltage |
| `battery_temp_dC` | `battery_temperature_dC` | 0.1°C | From BMS_25 broadcast |
| `balancing` | `balancing_active` | 0=init 1=active 2=inactive | |
| `charging_active` | `charging_active` | bool (0/1) | |
| `sdsw` | `SDSW` | bool (0/1) | Service disconnect switch missing |
| `pilotline` | `pilotline` | bool (0/1) | Pilotline open |
| `transport_mode` | `transportmode` | bool (0/1) | Transportation mode active |
| `component_protection` | `componentprotection` | bool (0/1) | |
| `shutdown_active` | `shutdown_active` | bool (0/1) | |
| `battery_heating` | `battery_heating` | bool (0/1) | |
| `bms_error_shutdown` | `BMS_error_shutdown` | bool (0/1) | |
| `bms_error_shutdown_req` | `BMS_error_shutdown_request` | bool (0/1) | |
| `bms_fault_performance` | `BMS_fault_performance` | bool (0/1) | |
| `welded_contactors` | `BMS_welded_contactors_status` | 0=Init 1=OK 2=Welded 3=Error | |

### MEB extended — BMS_28 realtime fault flags (`datalayer_extended.meb`)

All rt_* fields: 0=no fault, 1=error level 1, 2=error level 2, 3=error level 3.

| Key | Source field |
|---|---|
| `rt_overcurrent` | `rt_overcurrent` |
| `rt_can_fault` | `rt_CAN_fault` |
| `rt_overcharge` | `rt_overcharge` |
| `rt_soc_high` | `rt_SOC_high` |
| `rt_soc_low` | `rt_SOC_low` |
| `rt_soc_jumping` | `rt_SOC_jumping` |
| `rt_temp_difference` | `rt_temp_difference` |
| `rt_cell_overtemp` | `rt_cell_overtemp` |
| `rt_cell_undertemp` | `rt_cell_undertemp` |
| `rt_battery_overvolt` | `rt_battery_overvolt` |
| `rt_battery_undervolt` | `rt_battery_undervol` |
| `rt_cell_overvolt` | `rt_cell_overvolt` |
| `rt_cell_undervolt` | `rt_cell_undervol` |
| `rt_cell_imbalance` | `rt_cell_imbalance` |
| `rt_unauthorized` | `rt_battery_unathorized` |

---

## Implementation

### Core API
New files: `Software/src/devboard/serial/serial_api.h` and `serial_api.cpp`

- `serial_api_tick()` called from the main loop
- Returns immediately if `datalayer.system.info.usb_logging_active` is true
- Reads lines from `Serial`, parses `GET <key>`
- Looks up key in a flat registered table of `{name, getter}` pairs where getter is
  `int32_t(*)()`  (signed to accommodate negative values like current and temperature)
- Prints `<key>=<value>\n` or `ERR unknown_key\n`
- Stack-local line buffer, no heap allocation, no RTOS primitives

```cpp
typedef int32_t (*serial_api_getter_t)();
struct SerialApiKey { const char* name; serial_api_getter_t getter; };

void serial_api_register(const SerialApiKey* keys, size_t count);
void serial_api_tick();
```

### Registration model
The API has no knowledge of battery types. Keys are registered in two places:

1. **Core keys** — registered once at startup in `serial_api.cpp` itself. These cover
   `datalayer.battery.status` and `datalayer.system.status` fields that exist for every
   battery type.

2. **Battery-specific keys** — each battery module calls `serial_api_register()` from
   its `setup()` method, passing a static array of its own keys. The MEB battery
   registers all `datalayer_extended.meb.*` keys. A future battery registers its own.
   When a battery is not loaded, its keys are simply never registered and `GET` on them
   returns `ERR unknown_key` — which is correct, not a null sentinel, because the key
   genuinely does not exist for this configuration.

This eliminates the need for `is_applicable` guards entirely. The `null` response
concept from the commands section is therefore removed — unknown keys always return
`ERR unknown_key`.

### Table storage
A small fixed-size static array (e.g. 64 slots) is sufficient. Core keys use ~15 slots,
MEB battery-specific keys use ~35 slots, leaving headroom for future batteries.

`Software/src/Software.cpp` — add `serial_api_tick()` call in the main loop.

---

## Acceptance criteria

- **AC-1:** `GET soc` returns `soc=<digits>` within 2s when `usb_logging_active` is
  false (the default).
- **AC-2:** `GET unknown` returns `ERR unknown_key`.
- **AC-3:** All core keys return a correctly formatted `key=value` response regardless
  of active battery type.
- **AC-4:** MEB-specific keys return correct values when MEB is the active battery.
- **AC-5:** MEB-specific keys return `ERR unknown_key` when a non-MEB battery is active
  (keys not registered).
- **AC-6:** When `usb_logging_active` is true, `GET soc` receives no response within
  2s (API disabled).

---

## Files affected

- `Software/src/devboard/serial/serial_api.h` — new
- `Software/src/devboard/serial/serial_api.cpp` — new (core keys registered here)
- `Software/src/battery/MEB-BATTERY.cpp` — calls `serial_api_register()` from `setup()`
- `Software/src/Software.cpp` — add `serial_api_tick()` call
