#include "tmag5273.h"
#include "esphome/core/log.h"

namespace esphome::tmag5273 {

static const char *const TAG = "tmag5273";

bool TMAG5273Component::check_manufacturer_id_() {
  uint8_t mfg_lsb = 0;
  uint8_t mfg_msb = 0;
  if (!this->read_byte(REG_MANUFACTURER_ID_LSB, &mfg_lsb) ||
      !this->read_byte(REG_MANUFACTURER_ID_MSB, &mfg_msb)) {
    return false;
  }
  return mfg_lsb == MANUFACTURER_ID_LSB_VALUE && mfg_msb == MANUFACTURER_ID_MSB_VALUE;
}

void TMAG5273Component::setup() {
  // Identity check before touching anything else. Address 0x00 means auto:
  // probe the factory address of each variant (A/B/C/D) and keep the first
  // one that answers with TI's manufacturer ID.
  if (this->address_ == 0x00) {
    for (uint8_t candidate : VARIANT_ADDRESSES) {
      this->set_i2c_address(candidate);
      if (this->check_manufacturer_id_()) {
        ESP_LOGI(TAG, "Found TMAG5273 at address 0x%02X", candidate);
        break;
      }
      this->set_i2c_address(0x00);
    }
    if (this->address_ == 0x00) {
      ESP_LOGE(TAG, "No TMAG5273 found at any variant address (0x35/0x22/0x78/0x44)");
      this->mark_failed();
      return;
    }
  } else if (!this->check_manufacturer_id_()) {
    ESP_LOGE(TAG, "No TMAG5273 at configured address 0x%02X (manufacturer ID mismatch or no response)",
             this->address_);
    this->mark_failed();
    return;
  }

  if (!this->write_device_config_()) {
    this->mark_failed();
    return;
  }
}

bool TMAG5273Component::write_device_config_() {
  // DEVICE_CONFIG_1: CRC_EN=0, MAG_TEMPCO=0, CONV_AVG fixed at 8x, I2C_RD=0 (standard 3-byte read).
  uint8_t device_config_1 = 0;
  device_config_1 |= (CONV_AVG_8X & 0x07) << 2;
  if (!this->write_byte(REG_DEVICE_CONFIG_1, device_config_1)) {
    return false;
  }

  // DEVICE_CONFIG_2: LP_LN=0 (low-noise), TRIGGER_MODE=0 (I2C), OPERATING_MODE fixed continuous.
  uint8_t device_config_2 = 0;
  device_config_2 |= OPERATING_MODE_CONTINUOUS & 0x03;
  if (!this->write_byte(REG_DEVICE_CONFIG_2, device_config_2)) {
    return false;
  }

  // SENSOR_CONFIG_1: The channel pair follows the configured angle axes, sampled pseudo-simultaneously.
  MagChannels mag_channels = MAG_CH_NONE;
  switch (this->angle_en_) {
    case ANGLE_EN_XY:
      mag_channels = MAG_CH_XYX;
      break;
    case ANGLE_EN_YZ:
      mag_channels = MAG_CH_YZY;
      break;
    case ANGLE_EN_XZ:
      mag_channels = MAG_CH_XZX;
      break;
    default:
      break;
  }
  uint8_t sensor_config_1 = (static_cast<uint8_t>(mag_channels) & 0x0F) << 4;
  if (!this->write_byte(REG_SENSOR_CONFIG_1, sensor_config_1)) {
    return false;
  }

  // SENSOR_CONFIG_2: ANGLE_EN; ranges left at the low default, other bits
  // (threshold/gain) at default.
  uint8_t sensor_config_2 = (static_cast<uint8_t>(this->angle_en_) & 0x03) << 2;
  if (!this->write_byte(REG_SENSOR_CONFIG_2, sensor_config_2)) {
    return false;
  }

  return true;
}

void TMAG5273Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TMAG5273:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  ESP_LOGCONFIG(TAG, "  Angle pair (ANGLE_EN): %u", this->angle_en_);
}

bool TMAG5273Component::read_angle_raw(uint16_t *raw) {
  uint8_t buf[2] = {};
  if (!this->read_bytes(REG_ANGLE_RESULT_MSB, buf, sizeof(buf))) {
    return false;
  }
  // Top 3 bits are reserved (0). The combined 13-bit value carries 9 integer
  // bits + 4 fractional bits, so the raw register count is degrees * 16.
  *raw = static_cast<uint16_t>((buf[0] << 8) | buf[1]) & 0x1FFF;
  return true;
}

}  // namespace esphome::tmag5273
