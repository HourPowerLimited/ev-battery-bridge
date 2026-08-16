# SPEC-002 — End-to-End Integration Tests

## Status
Pending

## Dependencies
SPEC-001 (Bridge Serial API) must be done first.

## Background

The simulator has a layered integration test suite (`tests/integration/`) that proves
the full stack from serial API through CAN-FD bus to frame decoding. The bridge has no
equivalent. With the serial API from SPEC-001 in place, the same pattern applies: set a
value on the simulator, wait one CAN cycle, GET it from the bridge, assert they match.
No human, no browser, no WiFi required.

---

## Test layers

### Layer 1 — `bridge_serial_api`
**What it proves:** The bridge serial API responds correctly. No CAN bus needed.

**How it works:** Python opens COM3 with `dtr=False, rts=False` (no ESP32 reset),
sends `GET soc`, checks the response matches `soc=<digits>`.

**Pass condition:** Response received within 2s, format correct.

---

### Layer 2 — `bridge_can_rx`
**What it proves:** The bridge is receiving CAN frames from the simulator.

**How it works:** Simulator running production firmware on COM4. Python sends
`GET can_alive` to the bridge and checks the value is non-zero.

**Pass condition:** `can_alive` > 0 within 5s.

---

### Layer 3 — `bridge_meb_values`
**What it proves:** Known simulator values round-trip correctly through the CAN bus
into the bridge datalayer.

| Simulator SET | Bridge GET | Acceptance |
|---|---|---|
| `voltage=400` | `voltage_dV` | 3950 ≤ value ≤ 4050 |
| `soc=80` | `soc` | 7500 ≤ value ≤ 8500 |
| `maxChargePowerKw=50` | `max_charge_W` | 48000 ≤ value ≤ 52000 |
| `faultCellOvervoltage=2` | `rt_cell_overvolt` | value == 2 |
| `faultCellOvervoltage=0` | `rt_cell_overvolt` | value == 0 |
| `faultUnauthorized=1` | `rt_battery_unathorized` | value == 1 |
| `faultUnauthorized=0` | `rt_battery_unathorized` | value == 0 |

Each sub-test: SET on simulator → wait 1100ms (BMS_28 is 1000ms cycle) → GET from
bridge → assert.

---

## Test structure

Mirrors the simulator's `tests/integration/` layout exactly:

```
ev-battery-bridge/
  tests/
    integration/
      harness/
        __init__.py
        serial_io.py         # open_nodtr(), sim_set(), bridge_get()
        suite_bridge_api.py  # Layer 1
        suite_bridge_can.py  # Layer 2
        suite_bridge_meb.py  # Layer 3
        upload.py            # flash bridge production firmware to COM3
      README.md
      verify.py              # runner: --suite all/bridge_api/bridge_can/bridge_meb
```

### `serial_io.py` key points
- `open_nodtr(port, baud)` — opens with `dtr=False, rts=False`, sets `dtr=False`
  after open. Keeps port open across all sub-tests to avoid repeated resets.
- `sim_set(s, key, value)` — sends `SET <key> <value>\n` to simulator, reads `OK`
- `bridge_get(s, key, timeout=2)` — sends `GET <key>\n` to bridge, reads lines until
  `<key>=<value>` seen or timeout, returns int value or None

### `verify.py` invocation
```
python tests/integration/verify.py --suite all --port COM4 --port-b COM3
python tests/integration/verify.py --suite bridge_meb --port COM4 --port-b COM3 --no-upload
```

---

## Acceptance criteria

- **AC-1:** Layer 1 passes with bridge running, no simulator needed.
- **AC-2:** Layer 2 passes with both boards connected and simulator running.
- **AC-3:** All Layer 3 sub-tests pass.
- **AC-4:** `verify.py --suite all` exits 0.
- **AC-5:** `verify.py --suite all --no-upload` exits 0 when boards are already
  programmed (fast re-run, no flash step).

---

## Files affected

- `tests/integration/` — new directory tree (Python only, no firmware changes)
