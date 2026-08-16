# SPEC-002 — MEB Realtime Fault Flag → Safety Event Mapping

## Status
Pending

## Background

`BMS_28` realtime fault flags are decoded correctly into `datalayer_extended.meb.rt_*`
fields by `handle_incoming_can_frame()`. However `update_values()` never calls
`set_event()` / `clear_event()` based on them, so faults are only visible on the
"More battery info" extended page — they do not produce UI alerts, do not set
`system_status = FAULT`, and do not appear in the events log.

This spec adds the fault flag → event mapping entirely within `MEB-BATTERY.cpp`.
No other files are touched.

---

## Fault flag severity model

Each `realtime_*` field carries a 3-bit severity:
- `0` — no fault (clear the event)
- `1` or `2` — warning level (`EVENT_LEVEL_WARNING`)
- `3` — error level (`EVENT_LEVEL_ERROR`), must set `system_status = FAULT`

The existing `set_event(event, data)` / `clear_event(event)` infrastructure handles
level assignment via the event's registered level in `events.cpp`. For fault flags that
must produce an error at severity 3 and a warning at severity 1–2, two separate events
are needed (one warning-level, one error-level), or the data byte is used to carry
severity and the event is registered at the appropriate level.

Inspect `events.cpp` to confirm which existing events are registered at WARNING vs
ERROR level before choosing the mapping. Use existing events wherever semantically
correct. Only add new event types if no existing event fits.

---

## Fault flag → event mapping

| Fault field | Severity 1–2 event | Severity 3 event |
|---|---|---|
| `realtime_overcurrent_monitor` | `EVENT_CHARGE_LIMIT_EXCEEDED` or `EVENT_DISCHARGE_LIMIT_EXCEEDED` (use data=severity) | same, data=3 |
| `realtime_CAN_communication_fault` | `EVENT_CAN_CORRUPTED_WARNING` | `EVENT_CAN_CORRUPTED_WARNING` data=3 |
| `realtime_overcharge_warning` | `EVENT_CHARGE_LIMIT_EXCEEDED` | `EVENT_CHARGE_LIMIT_EXCEEDED` data=3 |
| `realtime_SOC_too_high` | `EVENT_BATTERY_FULL` | `EVENT_BATTERY_FULL` data=3 |
| `realtime_SOC_too_low` | `EVENT_BATTERY_EMPTY` | `EVENT_BATTERY_EMPTY` data=3 |
| `realtime_SOC_jumping_warning` | `EVENT_SOC_PLAUSIBILITY_ERROR` | `EVENT_SOC_PLAUSIBILITY_ERROR` data=3 |
| `realtime_temperature_difference_warning` | `EVENT_BATTERY_TEMP_DEVIATION_HIGH` | `EVENT_BATTERY_TEMP_DEVIATION_HIGH` data=3 |
| `realtime_cell_overtemperature_warning` | `EVENT_BATTERY_OVERHEAT` | `EVENT_BATTERY_OVERHEAT` data=3 |
| `realtime_cell_undertemperature_warning` | `EVENT_BATTERY_FROZEN` | `EVENT_BATTERY_FROZEN` data=3 |
| `realtime_battery_overvoltage_warning` | `EVENT_BATTERY_OVERVOLTAGE` | `EVENT_BATTERY_OVERVOLTAGE` data=3 |
| `realtime_battery_undervoltage_warning` | `EVENT_BATTERY_UNDERVOLTAGE` | `EVENT_BATTERY_UNDERVOLTAGE` data=3 |
| `realtime_cell_overvoltage_warning` | `EVENT_CELL_OVER_VOLTAGE` | `EVENT_CELL_CRITICAL_OVER_VOLTAGE` |
| `realtime_cell_undervoltage_warning` | `EVENT_CELL_UNDER_VOLTAGE` | `EVENT_CELL_CRITICAL_UNDER_VOLTAGE` |
| `realtime_cell_imbalance_warning` | `EVENT_CELL_DEVIATION_HIGH` | `EVENT_CELL_DEVIATION_HIGH` data=3 |
| `realtime_warning_battery_unathorized` | `EVENT_HVIL_FAILURE` | `EVENT_HVIL_FAILURE` data=3 |

**Before implementing:** open `events.cpp` and verify the registered level for each
event above. If an event is registered as WARNING-only, it cannot turn the UI red at
severity 3 — in that case use the data byte to carry severity and document the
limitation, or find a suitable ERROR-level event.

---

## Implementation

**File:** `Software/src/battery/MEB-BATTERY.cpp` — `update_values()` only.

Add a block after the existing `set_event(EVENT_HVIL_FAILURE, ...)` calls that
iterates the fault flags and calls `set_event` / `clear_event` accordingly.

Pattern for each flag:

```cpp
if (realtime_cell_overvoltage_warning >= 3) {
    set_event(EVENT_CELL_CRITICAL_OVER_VOLTAGE, realtime_cell_overvoltage_warning);
    clear_event(EVENT_CELL_OVER_VOLTAGE);
} else if (realtime_cell_overvoltage_warning >= 1) {
    set_event(EVENT_CELL_OVER_VOLTAGE, realtime_cell_overvoltage_warning);
    clear_event(EVENT_CELL_CRITICAL_OVER_VOLTAGE);
} else {
    clear_event(EVENT_CELL_OVER_VOLTAGE);
    clear_event(EVENT_CELL_CRITICAL_OVER_VOLTAGE);
}
```

Keep the block compact — no helper function needed for 15 flags.

---

## Acceptance criteria

- **AC-1:** Setting `faultCellOvervoltage = 1` on the simulator produces a warning-level
  event visible in the bridge events page within 1100 ms (BMS_28 is 1000 ms periodic).
- **AC-2:** Setting `faultCellOvervoltage = 3` on the simulator turns the bridge UI red
  (system_status = FAULT) within 1100 ms.
- **AC-3:** Clearing `faultCellOvervoltage` back to 0 clears the event and the UI
  returns to normal within 1100 ms.
- **AC-4:** Setting `faultCellUndertemp = 2` produces a warning event (`EVENT_BATTERY_FROZEN`
  or equivalent) within 1100 ms.
- **AC-5:** Setting `faultCellUndertemp = 3` turns the UI red within 1100 ms.
- **AC-6:** All 15 fault flags independently produce events at the correct level when
  set to severity 1 and severity 3 on the simulator.
- **AC-7:** No spurious events fire when all fault flags are 0.

---

## Testing

Use the simulator serial API to set individual fault flags:

```
# Set faultCellOvervoltage = 1 → expect warning event on bridge
# Set faultCellOvervoltage = 3 → expect UI red on bridge
# Set faultCellOvervoltage = 0 → expect UI normal on bridge
```

Capture bridge serial output with:
```
python tools/serial_capture.py --port COM3 --secs 15
```

Check bridge web UI events page for event entries.

All ACs must pass before committing.

---

## Files affected

- `Software/src/battery/MEB-BATTERY.cpp` — `update_values()` additions only
