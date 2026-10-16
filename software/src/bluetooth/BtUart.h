#pragma once
#include <Arduino.h>
#include "uart.h"

// The Bluetooth module's UART, with buffers sized for it. The framework's
// Uart takes its buffer sizes from SERIAL_RX/TX_BUFFER_SIZE (platformio.ini),
// one setting for every port, and the debug serial needs a 1 KB TX buffer for
// the monitor lines: as a Uart, this port had 1 KB of TX for commands of at
// most 32 bytes. Same interrupt-driven rings as the framework's Uart
// (cores/arduino/Serial.cpp), on the same serial_t / uart.c driver.
// Only included by Bluetooth.hpp (main.cpp).
class BtUart : public Print {
public:
  static const uint16_t RX_SIZE = 256; // the ~200 B burst on reconnect
  static const uint16_t TX_SIZE = 64;  // commands: BtModel::CMD_LEN + "\r\n"

  BtUart(uint32_t rxPin, uint32_t txPin) {
    memset(&serial, 0, sizeof(serial));
    serial.pin_rx = digitalPinToPinName(rxPin);
    serial.pin_tx = digitalPinToPinName(txPin);
    serial.pin_rts = NC;
    serial.pin_cts = NC;
    serial.rx_buff = rxBuffer;
    serial.tx_buff = txBuffer;
  }

  bool begin(uint32_t baud) {
    if (!uart_init(&serial, baud, UART_WORDLENGTH_8B, UART_PARITY_NONE,
                   UART_STOPBITS_1, false, false, false)) {
      return false;
    }
    uart_attach_rx_callback(&serial, rxComplete);
    return true;
  }

  int available() const {
    return (RX_SIZE + serial.rx_head - serial.rx_tail) % RX_SIZE;
  }

  int read() {
    if (serial.rx_head == serial.rx_tail) {
      return -1;
    }
    unsigned char c = rxBuffer[serial.rx_tail];
    serial.rx_tail = (serial.rx_tail + 1) % RX_SIZE;
    return c;
  }

  using Print::write;
  size_t write(uint8_t c) override { return write(&c, 1); }

  // Waits for room when the ring is full (a line longer than TX_SIZE, typed
  // on the console): the interrupt drains it at ~11 bytes per ms.
  size_t write(const uint8_t *buffer, size_t size) override {
    size_t left = size;
    while (left > 0) {
      size_t room;
      while ((room = txRoom()) == 0) {
      }
      size_t n = min(left, min(room, (size_t)(TX_SIZE - serial.tx_head)));
      memcpy(&txBuffer[serial.tx_head], buffer, n);
      buffer += n;
      left -= n;
      serial.tx_head = (serial.tx_head + n) % TX_SIZE;
      // Idle: send this chunk (it starts at tx_tail, which caught up with
      // the old head). Busy: txComplete() picks it up.
      if (!serial_tx_active(&serial)) {
        serial.tx_size = n;
        uart_attach_tx_callback(&serial, txComplete, n);
      }
    }
    return size;
  }

private:
  // The interrupt callbacks only get this: it points at the buffers.
  serial_t serial;
  uint8_t rxBuffer[RX_SIZE];
  uint8_t txBuffer[TX_SIZE];

  size_t txRoom() const {
    uint16_t head = serial.tx_head;
    uint16_t tail = serial.tx_tail;
    return head >= tail ? TX_SIZE - 1 - head + tail : tail - head - 1;
  }

  // A byte arrived: stored unless the ring is full (then dropped).
  static void rxComplete(serial_t *obj) {
    unsigned char c;
    if (uart_getc(obj, &c) == 0) {
      uint16_t next = (obj->rx_head + 1) % RX_SIZE;
      if (next != obj->rx_tail) {
        obj->rx_buff[obj->rx_head] = c;
        obj->rx_head = next;
      }
    }
  }

  // The last transfer is done: send what was queued meanwhile, up to the end
  // of the ring (the HAL doesn't wrap).
  static int txComplete(serial_t *obj) {
    obj->tx_tail = (obj->tx_tail + obj->tx_size) % TX_SIZE;
    if (obj->tx_head != obj->tx_tail) {
      size_t queued = (TX_SIZE + obj->tx_head - obj->tx_tail) % TX_SIZE;
      obj->tx_size = min(queued, (size_t)(TX_SIZE - obj->tx_tail));
      uart_attach_tx_callback(obj, txComplete, obj->tx_size);
      return -1;
    }
    return 0;
  }
};
