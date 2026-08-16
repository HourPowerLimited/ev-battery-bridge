# SPEC-003 — BMS_28 DLC-14 Decode Verification

## Status
Done

## Background

`BMS_28` is transmitted by the simulator with DLC code 14 (= 48 bytes payload in
CAN-FD). The bridge's MCP2518FD driver converts DLC codes to byte lengths via
`dlcToLen()`, so `rx_frame.DLC` in `handle_incoming_can_frame()` should be 48 when
the frame arrives.

The fault fields are packed across bytes 2–7 and byte 40. If `dlcToLen()` is not
applied before the frame reaches the switch statement, or if the frame is silently
dropped because its DLC code is unexpected, all fault fields will read as zero
regardless of what the simulator sends — which would be indistinguishable from
"no faults" and would mask GAP-4 entirely.

This spec verifies the decode path is correct before SPEC-004 (fault events) is
considered done.

---

## What to verify

1. **Driver DLC conversion** — confirm `comm_can_mcp2518fd.cpp` calls `dlcToLen()`
   and stores the byte count (not the DLC code) in `rx_frame.DLC` before passing the
   frame to `handle_incoming_can_frame()`.

2. **Frame not dropped** — confirm there is no length or DLC guard in
   `handle_incoming_can_frame()` or the CAN receive path that would silently discard
   a 48-byte frame.

3. **Byte 40 reachable** — confirm that `rx_frame.data.u8[40]` is within the allocated
   `CAN_frame` data buffer. Check the `CAN_frame` struct definition for the data array
   size.

4. **End-to-end decode** — with both boards connected, set `faultUnauthorized = 1` on
   the simulator via serial API and confirm the value appears in
   `datalayer_extended.meb.rt_battery_unathorized` on the bridge (visible on the
   "More battery info" extended page, or via serial log).

---

## Acceptance criteria

- **AC-1:** `comm_can_mcp2518fd.cpp` stores byte count (not DLC code) in
  `rx_frame.DLC` for CAN-FD frames. Verified by code inspection.
- **AC-2:** `CAN_frame.data` buffer is ≥ 48 bytes. Verified by code inspection of the
  struct definition.
- **AC-3:** Setting `faultUnauthorized = 1` on the simulator is reflected as
  `rt_battery_unathorized = 1` on the bridge extended page within 1100 ms.
- **AC-4:** Setting `faultCellOvervoltage = 2` on the simulator is reflected as
  `rt_cell_overvolt = 2` on the bridge extended page within 1100 ms.
- **AC-5:** Setting all fault fields to 0 on the simulator results in all
  `rt_*` fields reading 0 on the bridge extended page.

---

## Implementation

This spec is primarily a verification task. If AC-1 or AC-2 fail, a code fix is
required:

- **If DLC code is stored instead of byte count:** fix `comm_can_mcp2518fd.cpp` to
  call `dlcToLen()` before assigning `rx_frame.DLC`.
- **If the data buffer is too small:** increase the buffer size in the `CAN_frame`
  struct (upstream file — minimal touch, document in commit message).

If AC-3–AC-5 fail after AC-1 and AC-2 pass, the fault field bit-packing in
`handle_incoming_can_frame()` case `BMS_28` is incorrect — cross-reference against
`meb_frames.cpp` `makeBMS28()` to find the mismatch.

---

## Testing

AC-1 and AC-2 verified by code inspection.

AC-3–AC-5 verified via the bridge web UI (`/advanced` page) after setting fault
fields on the simulator. The `serial_capture.py` tool is not suitable for runtime
log capture because opening COM3 asserts DTR and resets the ESP32. Use the web UI
debug log (requires `usb_logging_active` enabled) or syslog instead.

rt_* field changes are now logged via `logging.printf` in `update_values()` —
visible on the web debug log and syslog whenever a field transitions.

---

## Files affected

- `Software/src/communication/can/comm_can_mcp2518fd.cpp` — no change needed (AC-1 already correct)
- Upstream `CAN_frame` struct — no change needed (AC-2 already correct, buffer is 64 bytes)
- `Software/src/battery/MEB-BATTERY.cpp` — added rt_* change-detection logging in `update_values()`
