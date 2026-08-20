#include "tmag5273_sensor.h"
#include "esphome/core/log.h"

namespace esphome::tmag5273 {

static const char *const TAG = "tmag5273.sensor";

// Angle data layout (datasheet section 6.5.2.3): 13-bit value carrying a
// 9-bit integer degree count plus 4 fractional bits (LSB = 1/16°).
static constexpr float ANGLE_LSB_PER_DEG = 16.0f;

void TMAG5273Sensor::dump_config() {
  LOG_SENSOR("", "TMAG5273 Angle", this);
  LOG_UPDATE_INTERVAL(this);
}

void TMAG5273Sensor::update() {
  TMAG5273Component *parent = this->parent_;
  if (parent->is_failed()) {
    return;
  }

  uint16_t raw = 0;
  if (!parent->read_angle_raw(&raw)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->publish_state(static_cast<float>(raw) / ANGLE_LSB_PER_DEG);
}

}  // namespace esphome::tmag5273
