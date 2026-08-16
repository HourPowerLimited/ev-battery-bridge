#include "serial_api.h"
#include <Arduino.h>
#include "../../datalayer/datalayer.h"

// ---------------------------------------------------------------------------
// Internal block table
// ---------------------------------------------------------------------------

struct KeyBlock {
  const SerialApiKey* keys;
  size_t count;
};

static KeyBlock s_blocks[8];
static size_t s_block_count = 0;

void serial_api_register(const SerialApiKey* keys, size_t count) {
  if (s_block_count < 8) {
    s_blocks[s_block_count++] = {keys, count};
  }
}

// ---------------------------------------------------------------------------
// Core keys — always registered, every battery type
// ---------------------------------------------------------------------------

// clang-format off
static const SerialApiKey core_keys[] = {
  { "soc",             []() -> int32_t { return datalayer.battery.status.real_soc; } },
  { "soh",             []() -> int32_t { return datalayer.battery.status.soh_pptt; } },
  { "voltage_dV",      []() -> int32_t { return datalayer.battery.status.voltage_dV; } },
  { "current_dA",      []() -> int32_t { return datalayer.battery.status.current_dA; } },
  { "power_W",         []() -> int32_t { return datalayer.battery.status.active_power_W; } },
  { "max_charge_W",    []() -> int32_t { return (int32_t)datalayer.battery.status.max_charge_power_W; } },
  { "max_discharge_W", []() -> int32_t { return (int32_t)datalayer.battery.status.max_discharge_power_W; } },
  { "remaining_Wh",    []() -> int32_t { return (int32_t)datalayer.battery.status.remaining_capacity_Wh; } },
  { "cell_min_mV",     []() -> int32_t { return datalayer.battery.status.cell_min_voltage_mV; } },
  { "cell_max_mV",     []() -> int32_t { return datalayer.battery.status.cell_max_voltage_mV; } },
  { "temp_min_dC",     []() -> int32_t { return datalayer.battery.status.temperature_min_dC; } },
  { "temp_max_dC",     []() -> int32_t { return datalayer.battery.status.temperature_max_dC; } },
  { "can_alive",       []() -> int32_t { return datalayer.battery.status.CAN_battery_still_alive; } },
  { "bms_status",      []() -> int32_t { return datalayer.battery.status.real_bms_status; } },
  { "system_status",   []() -> int32_t { return datalayer.system.status.system_status; } },
};
// clang-format on

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

static void handle_get(const char* key) {
  for (size_t b = 0; b < s_block_count; b++) {
    for (size_t i = 0; i < s_blocks[b].count; i++) {
      if (strcmp(s_blocks[b].keys[i].name, key) == 0) {
        Serial.print(s_blocks[b].keys[i].name);
        Serial.print('=');
        Serial.println(s_blocks[b].keys[i].getter());
        return;
      }
    }
  }
  Serial.println("ERR unknown_key");
}

void serial_api_tick() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    serial_api_register(core_keys, sizeof(core_keys) / sizeof(core_keys[0]));
  }

  if (datalayer.system.info.usb_logging_active) {
    return;
  }

  if (!Serial.available()) {
    return;
  }

  static char buf[48];
  static size_t buf_len = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      // trim trailing \r
      if (buf_len > 0 && buf[buf_len - 1] == '\r') {
        buf_len--;
      }
      buf[buf_len] = '\0';

      if (strncmp(buf, "GET ", 4) == 0) {
        handle_get(buf + 4);
      } else if (buf_len > 0) {
        Serial.println("ERR unknown_command");
      }

      buf_len = 0;
    } else if (buf_len < sizeof(buf) - 1) {
      buf[buf_len++] = c;
    }
  }
}
