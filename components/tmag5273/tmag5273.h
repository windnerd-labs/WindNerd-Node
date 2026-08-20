#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::tmag5273 {

// Register addresses (8-bit). Result registers are paired MSB/LSB.
static const uint8_t REG_DEVICE_CONFIG_1 = 0x00;
static const uint8_t REG_DEVICE_CONFIG_2 = 0x01;
static const uint8_t REG_SENSOR_CONFIG_1 = 0x02;
static const uint8_t REG_SENSOR_CONFIG_2 = 0x03;
static const uint8_t REG_MANUFACTURER_ID_LSB = 0x0E;
static const uint8_t REG_MANUFACTURER_ID_MSB = 0x0F;
static const uint8_t REG_ANGLE_RESULT_MSB = 0x19;

// Expected identification values.
static const uint8_t MANUFACTURER_ID_LSB_VALUE = 0x49;
static const uint8_t MANUFACTURER_ID_MSB_VALUE = 0x54;

// Factory default addresses of the four orderable variants (TMAG5273A/B/C/D).
// Scanned in this order when the configured address is 0x00 (= auto).
static const uint8_t VARIANT_ADDRESSES[] = {0x35, 0x22, 0x78, 0x44};

// OPERATING_MODE (DEVICE_CONFIG_2 bits 1:0): fixed to continuous conversion;
// not exposed as a config option.
static const uint8_t OPERATING_MODE_CONTINUOUS = 2;

// CONV_AVG (DEVICE_CONFIG_1 bits 4:2): samples averaged per reading. Fixed at
// 8x -- a solid noise/latency tradeoff for angle data; not exposed as a config option.
static const uint8_t CONV_AVG_8X = 3;

// MAG_CH_EN (SENSOR_CONFIG_1 bits 7:4). Only the pseudo-simultaneous
// selections are used -- TI recommends them for angle accuracy.
enum MagChannels : uint8_t {
  MAG_CH_NONE = 0x0,
  MAG_CH_XYX = 0x8,
  MAG_CH_YZY = 0xA,
  MAG_CH_XZX = 0xB,
};

// ANGLE_EN (SENSOR_CONFIG_2 bits 3:2).
enum AngleEn : uint8_t {
  ANGLE_EN_NONE = 0,
  ANGLE_EN_XY = 1,
  ANGLE_EN_YZ = 2,
  ANGLE_EN_XZ = 3,
};

class TMAG5273Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Which angle pair the device measures (a device-level setting; only one
  // pair can be active at a time). Set from the hub's codegen.
  void set_angle_en(AngleEn a) { this->angle_en_ = a; }

  // Low-level read used by the sensor sub-platform during update().
  // Returns false on I2C failure.
  bool read_angle_raw(uint16_t *raw);

 protected:
  bool write_device_config_();
  bool check_manufacturer_id_();

  AngleEn angle_en_{ANGLE_EN_NONE};
};

}  // namespace esphome::tmag5273
