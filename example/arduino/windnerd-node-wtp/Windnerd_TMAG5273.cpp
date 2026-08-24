/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_TMAG5273.h"

// Registers (8-bit). Result registers are paired MSB/LSB.
#define REG_DEVICE_CONFIG_1 0x00
#define REG_DEVICE_CONFIG_2 0x01
#define REG_SENSOR_CONFIG_1 0x02
#define REG_SENSOR_CONFIG_2 0x03
#define REG_MANUFACTURER_ID_LSB 0x0E
#define REG_MANUFACTURER_ID_MSB 0x0F
#define REG_ANGLE_RESULT_MSB 0x19

// Expected identification values.
#define MANUFACTURER_ID_LSB_VALUE 0x49
#define MANUFACTURER_ID_MSB_VALUE 0x54

// CONV_AVG (DEVICE_CONFIG_1 bits 4:2): 8x samples averaged per reading
#define CONV_AVG_8X 3
// OPERATING_MODE (DEVICE_CONFIG_2 bits 1:0).
#define OPERATING_MODE_CONTINUOUS 2
// MAG_CH_EN (SENSOR_CONFIG_1 bits 7:4): X and Z sampled pseudo-simultaneously
#define MAG_CH_XZX 0xB
// ANGLE_EN (SENSOR_CONFIG_2 bits 3:2): compute the angle from the X/Z pair.
#define ANGLE_EN_XZ 3

#define ANGLE_LSB_PER_DEG 16.0f

// Factory addresses of the four orderable variants (TMAG5273A/B/C/D)
static const uint8_t VARIANT_ADDRESSES[] = {0x22, 0x35, 0x78, 0x44};

bool WN_TMAG5273::checkManufacturerId(uint8_t addr)
{
  uint8_t mfg_lsb = 0;
  uint8_t mfg_msb = 0;
  if (_i2c.readRegister(addr, REG_MANUFACTURER_ID_LSB, &mfg_lsb, 1) != WN_I2C_OK ||
      _i2c.readRegister(addr, REG_MANUFACTURER_ID_MSB, &mfg_msb, 1) != WN_I2C_OK)
  {
    return false;
  }
  return mfg_lsb == MANUFACTURER_ID_LSB_VALUE && mfg_msb == MANUFACTURER_ID_MSB_VALUE;
}

bool WN_TMAG5273::begin(uint8_t scl_pin, uint8_t sda_pin)
{
  _i2c.begin(scl_pin, sda_pin);

  _address = 0;
  for (uint8_t i = 0; i < sizeof(VARIANT_ADDRESSES); i++)
  {
    if (checkManufacturerId(VARIANT_ADDRESSES[i]))
    {
      _address = VARIANT_ADDRESSES[i];
      break;
    }
  }
  if (_address == 0)
  {
    return false;
  }

  return writeDeviceConfig();
}

bool WN_TMAG5273::writeDeviceConfig()
{
  // DEVICE_CONFIG_1: CRC_EN=0, MAG_TEMPCO=0, CONV_AVG 8x, I2C_RD=0 (standard read).
  if (_i2c.writeRegister(_address, REG_DEVICE_CONFIG_1, (CONV_AVG_8X & 0x07) << 2) != WN_I2C_OK)
    return false;

  // DEVICE_CONFIG_2: LP_LN=0 (low-noise), TRIGGER_MODE=0 (I2C), continuous conversion.
  if (_i2c.writeRegister(_address, REG_DEVICE_CONFIG_2, OPERATING_MODE_CONTINUOUS & 0x03) != WN_I2C_OK)
    return false;

  // SENSOR_CONFIG_1: enable the X/Z channel pair.
  if (_i2c.writeRegister(_address, REG_SENSOR_CONFIG_1, (MAG_CH_XZX & 0x0F) << 4) != WN_I2C_OK)
    return false;

  // SENSOR_CONFIG_2: angle from X/Z; ranges and thresholds left at their defaults.
  if (_i2c.writeRegister(_address, REG_SENSOR_CONFIG_2, (ANGLE_EN_XZ & 0x03) << 2) != WN_I2C_OK)
    return false;

  return true;
}

bool WN_TMAG5273::readAngle(float *angle_deg)
{
  if (_address == 0)
    return false;

  uint8_t buf[2];
  if (_i2c.readRegister(_address, REG_ANGLE_RESULT_MSB, buf, 2) != WN_I2C_OK)
    return false;

  // Top 3 bits are reserved (0).
  uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]) & 0x1FFF;
  *angle_deg = (float)raw / ANGLE_LSB_PER_DEG;
  return true;
}
