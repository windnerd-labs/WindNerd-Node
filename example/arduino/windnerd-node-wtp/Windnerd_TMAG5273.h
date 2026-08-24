/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include "Windnerd_I2C.h"

// The Node is mains powered, so the sensor is left in continuous-conversion
// mode
class WN_TMAG5273
{
public:
  // Initializes the bus and configures the sensor. Probes the factory address
  // of each orderable variant (A/B/C/D) and keeps the first one that answers
  // with TI's manufacturer ID. Returns false if no sensor was found.
  bool begin(uint8_t scl_pin, uint8_t sda_pin);

  // Reads the vane angle in degrees (0..359.94, 1/16 deg resolution).
  // Returns false on I2C failure, leaving *angle_deg untouched.
  bool readAngle(float *angle_deg);

  // I2C address the sensor answered on, 0 if begin() failed.
  uint8_t address() const { return _address; }

private:
  bool checkManufacturerId(uint8_t addr);
  bool writeDeviceConfig();

  WN_I2C _i2c;
  uint8_t _address = 0;
};
