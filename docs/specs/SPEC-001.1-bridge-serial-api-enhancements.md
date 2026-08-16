# SPEC-001.1 — Bridge Serial API Enhancements

## Status
Pending

## Dependencies
SPEC-001 (Bridge Serial API) must be done and flashed.

## Background

After SPEC-001 shipped, three enhancements were identified during smoke testing:

1. **Startup announcement** — when the API is active there is no indication on the
   serial port. If `usb_logging_active` was previously enabled via the web UI and
   persisted to NVM, the API silently does nothing after reboot with no way to diagnose
   it from the serial port alone.

2. **`GET descriptor`** — the test harness currently has to know the key list from the
   spec. A `GET descriptor` command that returns all registered key names lets the
   harness discover available keys at runtime, making tests self-describing and
   resilient to future key additions.

3. **`SET` for bridge runtime settings** — a small number of bridge-side values are
   useful to set from the test harness without touching the web UI. These are
   runtime-only (not persisted to NVM) so they are safe to expose: they reset on
   reboot, cannot corrupt stored configuration, and do not affect the battery side.

---

## Changes

### 1. Startup announcement

On boot, before entering the main loop, print one line to serial:

```
serial API ready\n
```

This line is printed **regardless** of `usb_logging_active`. It is the only output
the API ever produces without a preceding GET command. If logging is on, this line
appears in the log stream and is harmless. If logging is off, it confirms the API
is active.

If `usb_logging_active` is true at boot, follow with:

```
serial API disabled (usb logging active)\n
```

So a user who opens a terminal immediately after boot always knows which mode the
port is in.

### 2. `GET descriptor`

Returns a space-separated list of all currently registered key names on a single line:

```
GET descriptor    ->  descriptor=soc soh voltage_dV current_dA ... bms_mode rt_overcurrent ...\n
```

The harness can use this to assert that expected keys are present after a battery
registers, and to enumerate keys for exhaustive testing without hardcoding the list.

Implementation: iterate all registered blocks, print each key name separated by
spaces, prefixed with `descriptor=`.

### 3. Runtime-only `SET`

Adds a `SET <key> <value>` command for a small fixed list of bridge runtime settings.
These are **never** written to NVM — they reset on reboot.

```
SET <key> <value>    ->  OK\n             on success
                     ->  ERR unknown_key\n
                     ->  ERR bad_value\n   value out of range or wrong type
```

Initial writable keys:

| Key | Target | Type | Range | Notes |
|---|---|---|---|---|
| `soc_scaling` | `datalayer.battery.settings.soc_scaling_active` | bool (0/1) | 0 or 1 | Enables/disables SOC window scaling |
| `max_charge_W` | `datalayer.battery.status.max_charge_power_W` | uint32 | 0–200000 | Override charge limit for test |
| `max_discharge_W` | `datalayer.battery.status.max_discharge_power_W` | uint32 | 0–200000 | Override discharge limit for test |

These three are sufficient for SPEC-002 integration tests. The list grows in future
patch specs as test needs are identified. No battery-side values are writable.

Implementation: a separate static setter table `{name, setter}` where setter is
`void(*)(int32_t)`. Parsed alongside the getter table in `serial_api_tick()`.

---

## Acceptance criteria

- **AC-1:** On boot with `usb_logging_active` false, `serial API ready` appears on
  the serial port within 5s of power-on, before any GET command is sent.
- **AC-2:** On boot with `usb_logging_active` true, both `serial API ready` and
  `serial API disabled (usb logging active)` appear in the serial log stream.
- **AC-3:** `GET descriptor` returns a line starting with `descriptor=` containing
  all registered key names as a space-separated list.
- **AC-4:** `GET descriptor` with MEB battery active includes `bms_mode` and
  `rt_unauthorized` in the response.
- **AC-5:** `SET soc_scaling 1` returns `OK` and `GET soc_scaling` returns
  `soc_scaling=1`.
- **AC-6:** `SET soc_scaling 0` returns `OK` and `GET soc_scaling` returns
  `soc_scaling=0`.
- **AC-7:** `SET max_charge_W 50000` returns `OK` and `GET max_charge_W` returns
  `max_charge_W=50000`.
- **AC-8:** `SET max_charge_W 999999` returns `ERR bad_value` (out of range).
- **AC-9:** `SET unknown_key 1` returns `ERR unknown_key`.
- **AC-10:** SET values do not persist across reboot — after power cycle, values
  return to their pre-SET state.

---

## Integration tests

New suite `suite_bridge_api_11.py` added to `tests/integration/harness/`.
Run standalone or as part of `--suite all` after SPEC-002 ships.

### Test 1 — startup announcement
Open COM3, wait 10s, check that `serial API ready` appears in the received bytes.

### Test 2 — GET descriptor completeness
Send `GET descriptor`, parse the response, assert all core keys are present:
`soc`, `soh`, `voltage_dV`, `current_dA`, `power_W`, `max_charge_W`,
`max_discharge_W`, `remaining_Wh`, `cell_min_mV`, `cell_max_mV`, `temp_min_dC`,
`temp_max_dC`, `can_alive`, `bms_status`, `system_status`.

### Test 3 — GET descriptor MEB keys present
Assert `bms_mode`, `rt_cell_overvolt`, `rt_unauthorized` are in the descriptor
(MEB battery must be active).

### Test 4 — SET/GET round-trip
```
SET soc_scaling 1  -> OK
GET soc_scaling    -> soc_scaling=1
SET soc_scaling 0  -> OK
GET soc_scaling    -> soc_scaling=0
SET max_charge_W 75000  -> OK
GET max_charge_W        -> max_charge_W=75000
```

### Test 5 — SET error cases
```
SET max_charge_W 999999  -> ERR bad_value
SET nonexistent 1        -> ERR unknown_key
```

### Test 6 — SET does not persist
```
SET max_charge_W 12345  -> OK
# power-cycle bridge (not testable automatically — document as manual AC-10)
```
AC-10 is marked as manual verification only.

---

## Files affected

- `Software/src/devboard/serial/serial_api.h` — add `SerialApiSetter` struct and
  `serial_api_register_setters()` declaration
- `Software/src/devboard/serial/serial_api.cpp` — startup announcement, descriptor
  command, SET handling, setter table
- `tests/integration/harness/suite_bridge_api_11.py` — new test suite
- `tests/integration/verify.py` — add `bridge_api_11` to suite list
