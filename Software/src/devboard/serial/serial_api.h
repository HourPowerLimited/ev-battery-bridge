#pragma once
#include <stddef.h>
#include <stdint.h>

typedef int32_t (*serial_api_getter_t)();

struct SerialApiKey {
  const char* name;
  serial_api_getter_t getter;
};

// Register a block of keys. Called once from each battery's setup().
// Keys must point to a static array — the pointer is stored, not copied.
void serial_api_register(const SerialApiKey* keys, size_t count);

// Call from core_loop() on every iteration.
// No-op if usb_logging_active is true.
void serial_api_tick();
