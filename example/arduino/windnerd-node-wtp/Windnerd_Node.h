/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "Windnerd_Rolling_Buffer.h"
#include "Windnerd_Vector_Averager.h"
#include "Windnerd_TMAG5273.h"

// WindNerd Node board pinout (ESP32-C3)
#define NODE_SPEED_INPUT_PIN 7  // anemometer pulses
#define NODE_SDA_PIN 1          // I2C, shared with the extension connector
#define NODE_SCL_PIN 2
#define NODE_LED_PIN 20         // diagnostic LED

typedef struct
{
  float speed = 0;
  uint16_t dir = 0;
} wn_instant_wind_sample_t;

typedef struct
{
  float avg_speed = 0;
  uint16_t avg_dir = 0;
  float min_speed = 0;
  float max_speed = 0;
} wn_wind_report_t;

typedef enum
{
  UNIT_MS = 0,
  UNIT_KN,
  UNIT_KPH,
  UNIT_MPH
} wn_wind_unit_t;

// Wind sampling engine for the WindNerd Node, ported from the WindNerd Core
// library. Counts anemometer pulses and reads the vane at 10 Hz, publishes an
// instant wind sample every 3 seconds and a wind report every minute.
//
// begin() starts a FreeRTOS task that owns the sampling; nothing has to be
// called from loop(). The rolling buffer is mutex-protected, so the report and
// sample accessors are safe to call from another task (the WTP uploader does).
class WN_Node
{
public:
  WN_Node(
      uint8_t speed_input_pin = NODE_SPEED_INPUT_PIN,
      uint8_t scl_pin = NODE_SCL_PIN,
      uint8_t sda_pin = NODE_SDA_PIN);

  // Starts the sensors and the sampling task. Returns false if the wind vane
  // did not answer on the I2C bus; wind speed still works in that case.
  bool begin();

  // Called from the sampling task every 3 seconds with the latest sample.
  void onInstantWindUpdate(void (*cb)(wn_instant_wind_sample_t sample));
  // Called from the sampling task every minute with the last minute's report.
  void onNewWindReport(void (*cb)(wn_wind_report_t report));

  // Alternative rotor frequency to wind speed ratio (Hz to m/s), default 1.31.
  void setFrequencyToWindSpeedRatio(float ratio);
  // Degrees added to the raw vane angle so that 0 deg = true north.
  void setDirectionOffset(float offset_deg);

  // Wind report over `period` seconds, offset by `index` periods back from now.
  // index 0 is the most recent period, 1 the one before it, and so on.
  wn_wind_report_t computeReportForPeriodInSecIndexedFromLast(uint16_t period, uint16_t index, wn_wind_unit_t unit = UNIT_MS);
  // 3-second sample, indexed back from the most recent one.
  wn_instant_wind_sample_t getSampleIndexedFromLast(uint16_t index, wn_wind_unit_t unit = UNIT_MS);

  uint8_t getVaneAddress() { return _vane.address(); }

  // Most recent vane angle (deg, north-aligned), refreshed at 10 Hz by the
  // sampling task. NAN until the vane answers, so a dead vane stays visible.
  float getLatestVaneAngle() const { return _latest_vane_deg; }

private:
  static void samplerTask(void *arg);
  void tick();
  float pulsesToSpeed(float pulses, wn_wind_unit_t unit);
  wn_instant_wind_sample_t formatRawSample(wn_raw_wind_sample_t &raw_sample, wn_wind_unit_t unit);
  wn_wind_report_t formatRawReport(wn_raw_wind_report_t &raw_report, wn_wind_unit_t unit);

  uint8_t _speed_input_pin;
  uint8_t _scl_pin;
  uint8_t _sda_pin;
  float _HZ_to_ms;
  float _direction_offset = 0;
  volatile float _latest_vane_deg = NAN;
  uint32_t _ticks_cnt = 0;
  unsigned long _last_sampling_window_millis = 0;

  WN_TMAG5273 _vane;
  WN_ROLLINGBUFFER _rolling_buffer;
  WN_VECTOR_AVERAGER _vane_averager;
  SemaphoreHandle_t _buffer_mutex = nullptr;

  void (*_instant_wind_cb)(wn_instant_wind_sample_t sample) = nullptr;
  void (*_avg_wind_cb)(wn_wind_report_t report) = nullptr;
};
