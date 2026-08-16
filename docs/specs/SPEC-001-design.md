# SPEC-001 Design — Bridge Serial API

## Files to create / modify

```
Software/src/devboard/serial/serial_api.h        new
Software/src/devboard/serial/serial_api.cpp      new
Software/src/battery/MEB-BATTERY.cpp             add serial_api_register() call in setup()
Software/Software.cpp                            add serial_api_tick() call in core_loop()
```

---

## serial_api.h

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef int32_t (*serial_api_getter_t)();

struct SerialApiKey {
  const char* name;
  serial_api_getter_t getter;
};

// Register a block of keys. Called once from each battery's setup().
// Keys are appended to a fixed-size internal table; excess registrations are
// silently dropped (table is large enough that this should never happen).
void serial_api_register(const SerialApiKey* keys, size_t count);

// Call from core_loop() on every iteration.
// No-op if usb_logging_active is true.
void serial_api_tick();
```

---

## serial_api.cpp

### Internal table

```cpp
struct KeyBlock { const SerialApiKey* keys; size_t count; };
static KeyBlock s_blocks[8];
static size_t   s_block_count = 0;
```

Blocks are stored by pointer — no copying. Each battery passes a pointer to its own
`static const SerialApiKey[]` array, which lives in flash. Zero heap allocation.

### Internal table

```cpp
struct KeyBlock { const SerialApiKey* keys; size_t count; };
static KeyBlock s_blocks[8];
static size_t   s_block_count = 0;
```

### serial_api_register()

```cpp
void serial_api_register(const SerialApiKey* keys, size_t count) {
  if (s_block_count < 8) {
    s_blocks[s_block_count++] = { keys, count };
  }
}
```

### serial_api_tick()

Called from `core_loop()` on every iteration (runs as fast as possible, same cadence
as `receive_can()`). The function is cheap when there is no serial input — it checks
`Serial.available()` and returns immediately if zero.

```
if usb_logging_active → return
if no bytes available → return
read bytes into stack buffer until '\n' or buffer full
trim '\r' and '\n'
if line starts with "GET " → handle_get(line + 4)
else → Serial.println("ERR unknown_command")
```

Line buffer: `char buf[48]` on the stack. 48 bytes is enough for the longest key name
plus `"GET "` prefix with margin.

### handle_get()

```
for each registered block:
  for each key in block:
    if strcmp(key.name, query) == 0:
      Serial.print(key.name)
      Serial.print('=')
      Serial.println(key.getter())
      return
Serial.println("ERR unknown_key")
```

Linear scan over all registered keys. With ~50 keys total this is negligible — a few
hundred nanoseconds at most.

---

## Core key registration

Core keys are registered inside `serial_api_tick()` on first call via a
`static bool initialized` guard, so no changes to `setup()` are needed for the core
keys. This keeps the registration self-contained in `serial_api.cpp`.

```cpp
void serial_api_tick() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    static const SerialApiKey core_keys[] = {
      { "soc",          []() -> int32_t { return datalayer.battery.status.real_soc; } },
      { "soh",          []() -> int32_t { return datalayer.battery.status.soh_pptt; } },
      { "voltage_dV",   []() -> int32_t { return datalayer.battery.status.voltage_dV; } },
      { "current_dA",   []() -> int32_t { return datalayer.battery.status.current_dA; } },
      { "power_W",      []() -> int32_t { return datalayer.battery.status.active_power_W; } },
      { "max_charge_W", []() -> int32_t { return (int32_t)datalayer.battery.status.max_charge_power_W; } },
      { "max_discharge_W", []() -> int32_t { return (int32_t)datalayer.battery.status.max_discharge_power_W; } },
      { "remaining_Wh", []() -> int32_t { return (int32_t)datalayer.battery.status.remaining_capacity_Wh; } },
      { "cell_min_mV",  []() -> int32_t { return datalayer.battery.status.cell_min_voltage_mV; } },
      { "cell_max_mV",  []() -> int32_t { return datalayer.battery.status.cell_max_voltage_mV; } },
      { "temp_min_dC",  []() -> int32_t { return datalayer.battery.status.temperature_min_dC; } },
      { "temp_max_dC",  []() -> int32_t { return datalayer.battery.status.temperature_max_dC; } },
      { "can_alive",    []() -> int32_t { return datalayer.battery.status.CAN_battery_still_alive; } },
      { "bms_status",   []() -> int32_t { return datalayer.battery.status.real_bms_status; } },
      { "system_status",[]() -> int32_t { return datalayer.system.status.system_status; } },
    };
    serial_api_register(core_keys, sizeof(core_keys) / sizeof(core_keys[0]));
  }
  // ... rest of tick
}
```

Lambdas capturing nothing are zero-cost — they decay to plain function pointers.

---

## MEB battery key registration

In `MEB-BATTERY.cpp` `setup()`, after the existing init code:

```cpp
static const SerialApiKey meb_keys[] = {
  { "bms_mode",               []() -> int32_t { return datalayer_extended.meb.BMS_mode; } },
  { "hvil",                   []() -> int32_t { return datalayer_extended.meb.HVIL; } },
  { "isolation_kOhm",         []() -> int32_t { return (int32_t)datalayer_extended.meb.isolation_resistance; } },
  { "voltage_intermediate_dV",[]() -> int32_t { return datalayer_extended.meb.BMS_voltage_intermediate_dV; } },
  { "battery_temp_dC",        []() -> int32_t { return datalayer_extended.meb.battery_temperature_dC; } },
  { "balancing",              []() -> int32_t { return datalayer_extended.meb.balancing_active; } },
  { "charging_active",        []() -> int32_t { return datalayer_extended.meb.charging_active ? 1 : 0; } },
  { "sdsw",                   []() -> int32_t { return datalayer_extended.meb.SDSW ? 1 : 0; } },
  { "pilotline",              []() -> int32_t { return datalayer_extended.meb.pilotline ? 1 : 0; } },
  { "transport_mode",         []() -> int32_t { return datalayer_extended.meb.transportmode ? 1 : 0; } },
  { "component_protection",   []() -> int32_t { return datalayer_extended.meb.componentprotection ? 1 : 0; } },
  { "shutdown_active",        []() -> int32_t { return datalayer_extended.meb.shutdown_active ? 1 : 0; } },
  { "battery_heating",        []() -> int32_t { return datalayer_extended.meb.battery_heating ? 1 : 0; } },
  { "bms_error_shutdown",     []() -> int32_t { return datalayer_extended.meb.BMS_error_shutdown ? 1 : 0; } },
  { "bms_error_shutdown_req", []() -> int32_t { return datalayer_extended.meb.BMS_error_shutdown_request ? 1 : 0; } },
  { "bms_fault_performance",  []() -> int32_t { return datalayer_extended.meb.BMS_fault_performance ? 1 : 0; } },
  { "welded_contactors",      []() -> int32_t { return datalayer_extended.meb.BMS_welded_contactors_status; } },
  { "rt_overcurrent",         []() -> int32_t { return datalayer_extended.meb.rt_overcurrent; } },
  { "rt_can_fault",           []() -> int32_t { return datalayer_extended.meb.rt_CAN_fault; } },
  { "rt_overcharge",          []() -> int32_t { return datalayer_extended.meb.rt_overcharge; } },
  { "rt_soc_high",            []() -> int32_t { return datalayer_extended.meb.rt_SOC_high; } },
  { "rt_soc_low",             []() -> int32_t { return datalayer_extended.meb.rt_SOC_low; } },
  { "rt_soc_jumping",         []() -> int32_t { return datalayer_extended.meb.rt_SOC_jumping; } },
  { "rt_temp_difference",     []() -> int32_t { return datalayer_extended.meb.rt_temp_difference; } },
  { "rt_cell_overtemp",       []() -> int32_t { return datalayer_extended.meb.rt_cell_overtemp; } },
  { "rt_cell_undertemp",      []() -> int32_t { return datalayer_extended.meb.rt_cell_undertemp; } },
  { "rt_battery_overvolt",    []() -> int32_t { return datalayer_extended.meb.rt_battery_overvolt; } },
  { "rt_battery_undervolt",   []() -> int32_t { return datalayer_extended.meb.rt_battery_undervol; } },
  { "rt_cell_overvolt",       []() -> int32_t { return datalayer_extended.meb.rt_cell_overvolt; } },
  { "rt_cell_undervolt",      []() -> int32_t { return datalayer_extended.meb.rt_cell_undervol; } },
  { "rt_cell_imbalance",      []() -> int32_t { return datalayer_extended.meb.rt_cell_imbalance; } },
  { "rt_unauthorized",        []() -> int32_t { return datalayer_extended.meb.rt_battery_unathorized; } },
};
serial_api_register(meb_keys, sizeof(meb_keys) / sizeof(meb_keys[0]));
```

`meb_keys` is `static const` so it lives in flash and the pointer remains valid
forever. The lambda captures are empty so they are plain function pointers.

---

## Software.cpp change

In `core_loop()`, after `receive_can()` and `receive_rs485()`:

```cpp
receive_can();
receive_rs485();
serial_api_tick();   // <-- add this line
```

This runs on every core loop iteration (~1ms cadence). The function returns in
nanoseconds when there is no serial input.

---

## Threading

`serial_api_tick()` runs in `core_loop` (core task). `Serial.available()` and
`Serial.read()` are safe to call from any task on ESP32 Arduino. The datalayer reads
inside the getters are all single-word reads on naturally-aligned fields — no locking
needed.

---

## Block table sizing

| Source | Keys |
|---|---|
| Core (serial_api.cpp) | 15 |
| MEB battery | 32 |
| Headroom for future batteries | — |
| **Total blocks** | **2** (well within the 8-block limit) |

The block table stores pointers, not copies. Memory cost: 2 × 16 bytes = 32 bytes RAM.

---

## Wire protocol examples

```
→ GET soc\n
← soc=7823\n

→ GET rt_cell_overvolt\n
← rt_cell_overvolt=2\n

→ GET bms_mode\n
← bms_mode=1\n

→ GET rt_unauthorized\n          (non-MEB battery loaded)
← ERR unknown_key\n

→ GET garbage\n
← ERR unknown_key\n
```

---

## What is not in this design

- No `SET` command
- No `LIST` command (not needed for the test harness — keys are known from the spec)
- No framing beyond newlines (no length prefix, no checksum)
- No concurrency protection beyond what Arduino Serial provides
