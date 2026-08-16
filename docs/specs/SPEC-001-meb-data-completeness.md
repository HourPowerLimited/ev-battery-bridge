# SPEC-001 — MEB Data Completeness

## Status
Pending

## Background

The ev-battery-simulator transmits a faithful MEB CAN-FD bus. The ev-battery-bridge
receives those frames and populates its datalayer, which drives the web UI and the
safety layer. A cross-reference of every field the bridge's MEB driver reads against
what the simulator actually sends reveals several gaps where data either never arrives
or is blocked from reaching the UI.

---

## Gap Analysis

### GAP-1 — Voltage, Current, Power blocked by BMS mode guard

**Root cause:** In `MEB-BATTERY.cpp` `handle_incoming_can_frame()`, the `BMS_20` handler
only writes `BMS_current`, `BMS_voltage_intermediate`, and `BMS_voltage` when
`BMS_mode != BMS_TARGET_INIT` (mode 7). On first boot the simulator starts in mode 7.
The bridge sends `HVK_01` requesting HV, the simulator transitions to mode 1, but until
that first transition completes the voltage/current fields stay at their init values
(1480 raw = 370V, 16300 raw = 0A).

**Affected datalayer fields:**
- `voltage_dV` — shown on main UI page
- `current_dA` — shown on main UI page
- `active_power_W` — derived from voltage × current

**Expected behaviour:** Once the simulator transitions to mode 1 (HV Active), all three
should update immediately and track simulator changes in real time.

**Acceptance criteria:**
- AC-1: After mode transition to 1, `voltage_dV` reflects the simulator `voltage` field
  within one BMS_20 cycle (10 ms).
- AC-2: After mode transition to 1, `current_dA` reflects the simulator `current` field
  within one BMS_20 cycle.
- AC-3: `active_power_W` = `voltage_dV` × `current_dA` / 100 and updates accordingly.
- AC-4: Changing `voltage` on the simulator via serial API is reflected on the bridge UI
  within 100 ms.

**Fix location:** No code change needed — this is a timing/state issue. Verify by
checking the bridge web UI BMS mode field. If it shows mode 1, voltage should already
be working. If it shows mode 7, the HVK_01 handshake is not completing — investigate
why the simulator is not transitioning.

---

### GAP-2 — Temperature not populated (UDS dependency)

**Root cause:** The MEB driver reads temperature exclusively via UDS
`PID_MAX_TEMP` (0x1E0E) and `PID_MIN_TEMP` (0x1E0F), polled every 200 ms via ISO-TP.
The simulator has no UDS responder — `meb_profile.cpp` `receive()` only handles
`HVK_01` (0x503). All UDS requests from the bridge go unanswered. `battery_max_temp`
and `battery_min_temp` stay at their init value of 600, and the guard
`if ((battery_min_temp != 600) && (battery_max_temp != 600))` in `update_values()`
never passes, so `temperature_min_dC` and `temperature_max_dC` are never written.

**Note:** Temperature is also encoded in `BMS_11` (`actual_temperature_highest_C`,
`actual_temperature_lowest_C`) and `BMS_25` (`battery_temperature_dC`), but the bridge
only uses those fields for display in the extended MEB page — the safety layer reads
`temperature_min_dC` / `temperature_max_dC` which come exclusively from the UDS path.

**Affected datalayer fields:**
- `temperature_min_dC` — shown on main UI, used by safety layer for overheat/frozen events
- `temperature_max_dC` — shown on main UI, used by safety layer for overheat/frozen events

**Affected safety events (never fire without temperature):**
- `EVENT_BATTERY_OVERHEAT` — fires when `temperature_max_dC > BATTERY_MAXTEMPERATURE`
- `EVENT_BATTERY_FROZEN` — fires when `temperature_min_dC < BATTERY_MINTEMPERATURE`
- `EVENT_BATTERY_TEMP_DEVIATION_HIGH` — fires when max-min spread > threshold

**Acceptance criteria:**
- AC-5: The simulator responds to UDS `ReadDataByIdentifier` (0x22) requests for
  `PID_MAX_TEMP` (0x1E0E) and `PID_MIN_TEMP` (0x1E0F) with the current `tempMaxC` /
  `tempMinC` values from `BatteryState`.
- AC-6: Within 400 ms of the UDS responder being active, `temperature_max_dC` and
  `temperature_min_dC` on the bridge reflect the simulator values.
- AC-7: Changing `tempMaxC` on the simulator is reflected on the bridge UI within 600 ms
  (one 200 ms poll cycle + ISO-TP round trip).
- AC-8: Setting `tempMaxC` above 60°C triggers `EVENT_BATTERY_OVERHEAT` on the bridge.
- AC-9: Setting `tempMinC` below -20°C triggers `EVENT_BATTERY_FROZEN` on the bridge.

**Fix location:** `ev-battery-simulator` — add a UDS ISO-TP responder to
`meb_profile.cpp` that handles `ReadDataByIdentifier` for at minimum `PID_MAX_TEMP`
and `PID_MIN_TEMP`. The full PID list the bridge polls is documented in GAP-3.

---

### GAP-3 — All UDS-polled values missing (full PID list)

The bridge polls the following PIDs via UDS every 200 ms. None are answered by the
simulator today. Temperature (GAP-2) is the most visible, but the full list is:

| PID | Value | Bridge field |
|---|---|---|
| 0x028C | SOC (polled, higher accuracy) | `battery_soc_polled` → `real_soc` |
| 0x50CE | SOH | `battery_soh_polled` → `soh_pptt` |
| 0x1E3B | Pack voltage (polled) | `battery_voltage_polled` |
| 0x1E3D | Pack current (polled) | `battery_current_polled` |
| 0x1E0E | Max temperature | `battery_max_temp` → `temperature_max_dC` |
| 0x1E0F | Min temperature | `battery_min_temp` → `temperature_min_dC` |
| 0x5171 | Max charge voltage | `battery_max_charge_voltage` |
| 0x5170 | Min discharge voltage | `battery_min_discharge_voltage` |
| 0x1E32 | Energy counters (kWh charge/discharge) | `total_charged/discharged_battery_Wh` |
| 0x1E1B | Allowed charge power | `battery_allowed_charge_power` |
| 0x1E1C | Allowed discharge power | `battery_allowed_discharge_power` |
| 0x1E40–0x1EAB | Cell voltages 1–108 | `cellvoltages_polled[]` |
| 0x1EAE–0x1EBF | Temperature points 1–18 | `datalayer_extended.meb.temp_points[]` |

**Note:** The bridge uses `battery_soc_polled` from `PID_SOC` in preference to the
broadcast `battery_SOC` from `BMS_22` — so SOC accuracy also depends on UDS.

**Acceptance criteria:**
- AC-10: The simulator UDS responder handles all PIDs in the table above, returning
  values derived from the current `BatteryState`.
- AC-11: Cell voltages 1–108 are synthesised from `cellVoltageMinMv` and
  `cellVoltageMaxMv` with a linear spread across the cell count.
- AC-12: SOH is returned as a fixed 9500 (95.00%) unless a dedicated simulator control
  is added.

---

### GAP-4 — Realtime fault flags visible in extended page but do not trigger safety events

**Root cause:** `BMS_28` is received and decoded correctly — the `realtime_*` fields in
`datalayer_extended.meb` are populated. However, the bridge's safety layer
(`safety.cpp`) does not read these fields. Safety events (which turn the UI red) are
triggered only by the generic safety checks (cell voltage, pack voltage, temperature,
SOC limits) — not by the MEB-specific realtime fault flags.

The realtime faults are therefore visible only on the "More battery info" page, not as
red UI alerts.

**Affected fault flags (all in BMS_28):**
`faultOvercurrent`, `faultCANComms`, `faultOvercharge`, `faultSOCTooHigh`,
`faultSOCTooLow`, `faultSOCJumping`, `faultTempDifference`, `faultCellOvertemp`,
`faultCellUndertemp`, `faultBatteryOvervoltage`, `faultBatteryUndervoltage`,
`faultCellOvervoltage`, `faultCellUndervoltage`, `faultCellImbalance`,
`faultUnauthorized`

**Note:** Some of these overlap with generic safety checks that already fire (e.g.
`faultCellOvervoltage` would also be caught by the cell voltage check if the cell
voltage value is set accordingly). The gap is that the fault flag alone — without the
corresponding value crossing a threshold — does not trigger a UI alert.

**Acceptance criteria:**
- AC-13: Any realtime fault flag at severity ≥ 1 in `datalayer_extended.meb` triggers
  at minimum a warning-level event visible in the bridge events page.
- AC-14: Fault flags at severity 3 trigger an error-level event that sets
  `system_status = FAULT` and turns the UI red.
- AC-15: Setting `faultCellOvervoltage = 1` on the simulator is reflected as a warning
  event on the bridge within 1100 ms (BMS_28 is 1000 ms periodic).
- AC-16: Setting `faultCellOvervoltage = 3` on the simulator turns the bridge UI red.

**Fix location:** `ev-battery-bridge` — add fault flag → event mapping in
`MEB-BATTERY.cpp` `update_values()`, using existing `set_event()` / `clear_event()`
infrastructure.

---

### GAP-5 — BMS_28 DLC code 14 decode correctness

**Root cause:** The simulator sends `BMS_28` with DLC code 14 (= 48 bytes). The bridge
receives it and `rx_frame.DLC` will be 48 (byte count, not DLC code) after our driver's
`dlcToLen()` conversion. The fault fields at bytes 2–7 and byte 40 should decode
correctly. However, `m.data[40] = s.faultUnauthorized` in the simulator writes to
index 40 of the payload, which is within the 48-byte frame. This should be fine.

**Acceptance criteria:**
- AC-17: Setting `faultUnauthorized = 1` on the simulator is decoded correctly by the
  bridge (visible in extended page).
- AC-18: All fault fields in bytes 2–7 decode to the correct severity values.

---

## Implementation Order

1. **GAP-1** — Verify/diagnose the HVK_01 handshake. No code change expected.
2. **GAP-4** — Add fault flag → event mapping in the bridge. Self-contained, no
   simulator changes needed.
3. **GAP-5** — Verify BMS_28 decode correctness with a targeted serial API test.
4. **GAP-2 + GAP-3** — Add UDS ISO-TP responder to the simulator. This is the largest
   piece of work and unlocks temperature, polled SOC/voltage/current, cell voltages,
   and energy counters in one go.

---

## Files Affected

**ev-battery-bridge:**
- `Software/src/battery/MEB-BATTERY.cpp` — GAP-4: fault flag → event mapping

**ev-battery-simulator:**
- `firmware/src/profiles/meb/meb_profile.cpp` — GAP-2/3: UDS ISO-TP responder
- `firmware/src/profiles/meb/meb_frames.cpp` — GAP-2/3: UDS response frame builders
