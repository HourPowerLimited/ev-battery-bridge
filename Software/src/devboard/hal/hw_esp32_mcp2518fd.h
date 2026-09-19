#ifndef __HW_ESP32_MCP2518FD_H__
#define __HW_ESP32_MCP2518FD_H__

#include "hal.h"

/*
Bare ESP32 DevKit V1 + MCP2518FD breakout board.
Only the MCP2518FD SPI pins and a single INT pin are assumed to be wired.
Everything else is left as GPIO_NUM_NC — no LED, no contactors, no RS485.

Default wiring (VSPI bus):
  SCK  = GPIO 33
  MOSI = GPIO 32
  MISO = GPIO 35
  CS   = GPIO 25
  INT  = GPIO 34
*/

class Esp32Mcp2518fdHal : public Esp32Hal {
 public:
  const char* name() { return "ESP32 DevKit + MCP2518FD breakout"; }

  // CANFD via MCP2518FD on VSPI
  virtual gpio_num_t MCP2517_SCK() { return GPIO_NUM_33; }
  virtual gpio_num_t MCP2517_SDI() { return GPIO_NUM_32; }
  virtual gpio_num_t MCP2517_SDO() { return GPIO_NUM_35; }
  virtual gpio_num_t MCP2517_CS() { return GPIO_NUM_25; }
  virtual gpio_num_t MCP2517_INT() { return GPIO_NUM_34; }
  virtual uint32_t MCP2517_FREQ() { return 40000000; }

  std::vector<comm_interface> available_interfaces() {
    return {
        comm_interface::CanFdAddonMcp2518,
    };
  }
};

#define HalClass Esp32Mcp2518fdHal

/* ----- Error checks below, don't change ----- */
#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif  // __HW_ESP32_MCP2518FD_H__
