// comm_can_mcp2518fd.cpp
//
// Full implementation of the comm_can.h public API for HW_ESP32_MCP2518FD.
// Uses foodyfood/esp32-mcp2518fd-driver instead of ACAN2517FD.
//
// comm_can.cpp is completely untouched — this file is selected instead via
// src_filter in platformio.ini for the esp32_mcp2518fd env. On rebase,
// comm_can.cpp merges cleanly from upstream with zero conflicts.

#include "CanReceiver.h"
#include "comm_can.h"
#include "mcp2518fd_can.h"
#include "mcp2518fd_timing.h"
#include "src/datalayer/datalayer.h"
#include "src/devboard/hal/hal.h"
#include "src/devboard/safety/safety.h"
#include "src/devboard/sdcard/sdcard.h"
#include "src/devboard/utils/events.h"
#include "src/devboard/utils/logging.h"

#include <algorithm>
#include <map>

// ---------------------------------------------------------------------------
// Shared state (mirrors comm_can.cpp layout)
// ---------------------------------------------------------------------------

volatile CAN_Configuration can_config = {.battery = CANFD_ADDON_MCP2518,
                                         .inverter = CANFD_ADDON_MCP2518,
                                         .battery_double = CANFD_ADDON_MCP2518,
                                         .battery_triple = CANFD_ADDON_MCP2518,
                                         .charger = CANFD_ADDON_MCP2518,
                                         .shunt = CANFD_ADDON_MCP2518};

uint16_t user_selected_CAN_ID_cutoff_filter = 0;

struct CanReceiverRegistration {
  CanReceiver* receiver;
  CAN_Speed speed;
};

static std::multimap<CAN_Interface, CanReceiverRegistration> can_receivers;

static SPIClass* spi = nullptr;
static MCP2518Driver* canfd = nullptr;
static MCP2518Driver* canfd_2 = nullptr;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void print_can_frame(CAN_frame frame, CAN_Interface interface, frameDirection msgDir) {
  if (datalayer.system.info.CAN_usb_logging_active) {
    static char usb_line[288];
    static uint32_t usb_frames_dropped = 0;
    unsigned long currentTime = millis();
    size_t size = snprintf(usb_line, sizeof(usb_line), "(%lu.%02lu) %s%d %lX [%u] ", currentTime / 1000,
                           (currentTime % 1000) / 10, (msgDir == MSG_RX) ? "RX" : "TX",
                           (msgDir == MSG_RX) ? (int)(interface * 2) : (int)(interface * 2) + 1, frame.ID, frame.DLC);
    for (uint8_t i = 0; i < frame.DLC; i++) {
      size += snprintf(usb_line + size, sizeof(usb_line) - size, (i < frame.DLC - 1) ? "%02X " : "%02X\r\n",
                       frame.data.u8[i]);
    }
    if (frame.DLC == 0) {
      size += snprintf(usb_line + size, sizeof(usb_line) - size, "\r\n");
    }
    if ((size_t)Serial.availableForWrite() >= size) {
      if (usb_frames_dropped > 0) {
        char marker[48];
        int marker_len =
            snprintf(marker, sizeof(marker), "[%lu CAN frames not printed]\r\n", (unsigned long)usb_frames_dropped);
        if ((size_t)Serial.availableForWrite() >= size + (size_t)marker_len) {
          Serial.write((const uint8_t*)marker, marker_len);
          usb_frames_dropped = 0;
        }
      }
      Serial.write((const uint8_t*)usb_line, size);
    } else {
      usb_frames_dropped++;
    }
  }

  if (datalayer.system.info.can_logging_active) {
    if (frame.ID > user_selected_CAN_ID_cutoff_filter) {
      dump_can_frame(frame, interface, msgDir);
    }
  }
}

static void map_can_frame_to_variable(CAN_frame* rx_frame, CAN_Interface interface) {
  if (interface != CANFD_NATIVE) {
    print_can_frame(*rx_frame, interface, frameDirection(MSG_RX));
  }
#ifdef SDCARD
  if (datalayer.system.info.CAN_SD_logging_active) {
    if (interface != CANFD_NATIVE) {
      add_can_frame_to_buffer(*rx_frame, interface, frameDirection(MSG_RX));
    }
  }
#endif
  auto receivers = can_receivers.equal_range(interface);
  for (auto it = receivers.first; it != receivers.second; ++it) {
    it->second.receiver->receive_can_frame(rx_frame);
  }
}

static void receive_one(MCP2518Driver* drv, CAN_Interface iface) {
  int count = 0;
  while (drv->available() && count++ < 16) {
    CanMsg msg;
    drv->receive(msg);
    CAN_frame rx_frame;
    rx_frame.ID = msg.id;
    rx_frame.ext_ID = msg.ext;
    rx_frame.FD = msg.fdf;
    rx_frame.DLC = dlcToLen(msg.dlc);
    memcpy(rx_frame.data.u8, msg.data, rx_frame.DLC);
    map_can_frame_to_variable(&rx_frame, iface);
    if (iface == CANFD_ADDON_MCP2518) {
      map_can_frame_to_variable(&rx_frame, CANFD_NATIVE);
    }
  }
  if (drv->hasErrors()) {
    if (iface == CANFD_ADDON_MCP2518_2) {
      datalayer.system.info.can_2518_2_bus_error = true;
    } else {
      datalayer.system.info.can_2518_bus_error = true;
    }
  }
}

static bool init_one(MCP2518Driver*& drv, SPIClass& bus, gpio_num_t cs, gpio_num_t int_pin, CAN_Speed speed,
                     const char* label) {
  drv = new MCP2518Driver(bus, (uint8_t)cs, (int8_t)int_pin);
  uint32_t nominal = (uint32_t)speed * 1000UL;
  CanStatus status = drv->configure(nominal, nominal * 4, MODE_NORMAL);
  if (status != CanStatus::OK) {
    logging.print(label);
    logging.print(" init error: ");
    logging.println((uint8_t)status);
    set_event(EVENT_CANMCP2518FD_INIT_FAILURE, (uint8_t)status);
    delete drv;
    drv = nullptr;
    return false;
  }
  logging.print(label);
  logging.println(" ok");
  return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void register_can_receiver(CanReceiver* receiver, CAN_Interface interface, CAN_Speed speed) {
  can_receivers.insert({interface, {receiver, speed}});
  DEBUG_PRINTF("CAN receiver registered, total: %d\n", can_receivers.size());
}

bool init_CAN() {
  auto fdIt = can_receivers.find(CANFD_ADDON_MCP2518);
  auto fdIt_2 = can_receivers.find(CANFD_ADDON_MCP2518_2);

  if (fdIt == can_receivers.end() && fdIt_2 == can_receivers.end()) {
    return true;
  }

  auto sck_pin = esp32hal->MCP2517_SCK();
  auto sdo_pin = esp32hal->MCP2517_SDO();
  auto sdi_pin = esp32hal->MCP2517_SDI();

  if (!esp32hal->alloc_pins("CANFD", sck_pin, sdo_pin, sdi_pin)) {
    return false;
  }

  spi = new SPIClass(esp32hal->MCP2517_BUS());
  spi->begin(sck_pin, sdo_pin, sdi_pin);

  if (fdIt != can_receivers.end()) {
    auto cs = esp32hal->MCP2517_CS();
    auto irq = esp32hal->MCP2517_INT();
    if (!esp32hal->alloc_pins("CANFD", cs, irq)) {
      return false;
    }
    if (!init_one(canfd, *spi, cs, irq, fdIt->second.speed, "CAN FD (MCP2518FD)")) {
      return false;
    }
  }

  if (fdIt_2 != can_receivers.end()) {
    auto cs = esp32hal->MCP2517_CS2();
    auto irq = esp32hal->MCP2517_INT2();
    if (!esp32hal->alloc_pins("CANFD2", cs, irq)) {
      return false;
    }
    if (!init_one(canfd_2, *spi, cs, irq, fdIt_2->second.speed, "CAN FD 2 (MCP2518FD)")) {
      return false;
    }
  }

  return true;
}

void receive_can() {
  if (canfd)
    receive_one(canfd, CANFD_ADDON_MCP2518);
  if (canfd_2)
    receive_one(canfd_2, CANFD_ADDON_MCP2518_2);
}

void transmit_can_frame_to_interface(const CAN_frame* tx_frame, CAN_Interface interface) {
  if (!allowed_to_send_CAN) {
    return;
  }
  print_can_frame(*tx_frame, interface, frameDirection(MSG_TX));
#ifdef SDCARD
  if (datalayer.system.info.CAN_SD_logging_active) {
    add_can_frame_to_buffer(*tx_frame, interface, frameDirection(MSG_TX));
  }
#endif

  MCP2518Driver* drv = (interface == CANFD_ADDON_MCP2518_2) ? canfd_2 : canfd;
  if (drv == nullptr) {
    if (interface == CANFD_ADDON_MCP2518_2) {
      datalayer.system.info.can_2518_2_send_fail = true;
    } else {
      datalayer.system.info.can_2518_send_fail = true;
    }
    return;
  }

  CanMsg msg;
  msg.id = tx_frame->ID;
  msg.ext = tx_frame->ext_ID;
  msg.fdf = tx_frame->FD;
  msg.brs = tx_frame->FD;
  msg.dlc = lenToDlc(tx_frame->DLC);
  memcpy(msg.data, tx_frame->data.u8, tx_frame->DLC);

  if (drv->transmit(msg) != CanTxResult::OK) {
    if (interface == CANFD_ADDON_MCP2518_2) {
      datalayer.system.info.can_2518_2_send_fail = true;
    } else {
      datalayer.system.info.can_2518_send_fail = true;
    }
  }
}

void stop_can() {
  if (canfd)
    canfd->stop();
  if (canfd_2)
    canfd_2->stop();
}

void restart_can() {
  if (canfd)
    canfd->restart();
  if (canfd_2)
    canfd_2->restart();
}

bool change_can_speed(CAN_Interface interface, CAN_Speed speed) {
  // Speed changes on the FD interface require a full reconfigure — not supported at runtime.
  return false;
}

void dump_can_frame(CAN_frame& frame, CAN_Interface interface, frameDirection msgDir) {
  char* message_string = datalayer.system.info.logged_can_messages;
  int offset = datalayer.system.info.logged_can_messages_offset;
  size_t message_string_size = sizeof(datalayer.system.info.logged_can_messages);

  if (offset + 128 > sizeof(datalayer.system.info.logged_can_messages)) {
    offset = 0;
  }
  unsigned long currentTime = millis();
  offset += snprintf(message_string + offset, message_string_size - offset, "(%lu.%03lu) ", currentTime / 1000,
                     currentTime % 1000);
  offset += snprintf(message_string + offset, message_string_size - offset, "%s%d ", (msgDir == MSG_RX) ? "RX" : "TX",
                     (int)(interface * 2) + (msgDir == MSG_RX ? 0 : 1));
  offset += snprintf(message_string + offset, message_string_size - offset, "%lX [%u] ", frame.ID, frame.DLC);
  for (uint8_t i = 0; i < frame.DLC; i++) {
    if (i < frame.DLC - 1) {
      offset += snprintf(message_string + offset, message_string_size - offset, "%02X ", frame.data.u8[i]);
    } else {
      offset += snprintf(message_string + offset, message_string_size - offset, "%02X", frame.data.u8[i]);
    }
  }
  offset += snprintf(message_string + offset, message_string_size - offset, "\n");
  datalayer.system.info.logged_can_messages_offset = offset;
}
