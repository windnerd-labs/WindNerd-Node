/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include <Wire.h>

// Error codes returned by WN_I2C operations.
// 0..5 match TwoWire::endTransmission() return codes; 6 added by this layer.
#define WN_I2C_OK 0
#define WN_I2C_ERR_DATA_TOO_LONG 1
#define WN_I2C_ERR_NACK_ADDR 2
#define WN_I2C_ERR_NACK_DATA 3
#define WN_I2C_ERR_OTHER 4
#define WN_I2C_ERR_TIMEOUT 5
#define WN_I2C_ERR_SHORT_READ 6

// Thin wrapper around TwoWire that adds bus recovery and a write/read-register
// API with auto-recover-and-retry on failure.
class WN_I2C
{
public:
  WN_I2C(TwoWire &wire = Wire);


  void begin(uint8_t scl_pin, uint8_t sda_pin, uint32_t frequency = 100000);

  // Bitbang the bus to release a slave stuck holding SDA low. Pulses SCL up to
  // 9 times so the slave can finish the in-flight byte, then issues a STOP and
  // re-inits the underlying TwoWire.
  void recoverBus();

  uint8_t writeRegister(uint8_t addr, uint8_t reg, uint8_t data);
  uint8_t readRegister(uint8_t addr, uint8_t reg, uint8_t *data, size_t length);

  uint8_t lastError() const { return _last_error; }

private:
  TwoWire &_wire;
  uint8_t _scl_pin = 0;
  uint8_t _sda_pin = 0;
  uint32_t _frequency = 100000;
  uint8_t _last_error = WN_I2C_OK;
};
