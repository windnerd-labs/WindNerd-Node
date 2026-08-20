#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tmag5273/tmag5273.h"

namespace esphome::tmag5273 {

class TMAG5273Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public Parented<TMAG5273Component> {
 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::tmag5273
