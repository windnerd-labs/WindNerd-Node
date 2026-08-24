/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Node.h"

// frequency to speed ratio for the standard WindNerd rotor
#define HZ_TO_MS 1.31f

// 30 ticks at 10 Hz -> speed pulses are counted over 3 second windows, the WMO
// standard sampling basis for gusts.
#define TICK_HZ 10
#define SAMPLING_WINDOW_TICKS 30
#define SAMPLE_DURATION (SAMPLING_WINDOW_TICKS / TICK_HZ)

// one wind report per minute, which is also the WTP report interval
#define REPORT_PERIOD_SEC 60

// Minimum spacing between accepted pulses (us). Real rotor pulses stay well below
// this rate even in extreme winds, so anything faster is interference.
#define SPEED_PULSE_MIN_INTERVAL_US 10000

#define SAMPLER_TASK_STACK 4096
#define SAMPLER_TASK_PRIORITY 3

static volatile uint32_t speed_pulse_count = 0;  // incremented by rising edge interrupts on the speed input
static volatile uint32_t last_speed_pulse_us = 0;
static uint8_t isr_speed_input_pin = NODE_SPEED_INPUT_PIN;

static void IRAM_ATTR onSpeedPulseISR()
{
  // pin confirmation: reject sub-us glitches the edge latch saw but the line didn't hold
  if (digitalRead(isr_speed_input_pin) == LOW)
    return;

  // time-based debounce: reject pulses arriving faster than physically possible
  uint32_t now = micros();
  if (now - last_speed_pulse_us < SPEED_PULSE_MIN_INTERVAL_US)
    return;
  last_speed_pulse_us = now;

  speed_pulse_count = speed_pulse_count + 1;  // not ++: compound ops on volatile are deprecated in modern C++
}

WN_Node::WN_Node(uint8_t speed_input_pin, uint8_t scl_pin, uint8_t sda_pin)
    : _speed_input_pin(speed_input_pin),
      _scl_pin(scl_pin),
      _sda_pin(sda_pin),
      _HZ_to_ms(HZ_TO_MS)
{
  isr_speed_input_pin = _speed_input_pin;
}

bool WN_Node::begin()
{
  _buffer_mutex = xSemaphoreCreateMutex();

  bool vane_found = _vane.begin(_scl_pin, _sda_pin);

  pinMode(_speed_input_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(_speed_input_pin), onSpeedPulseISR, RISING);

  _last_sampling_window_millis = millis();
  xTaskCreate(samplerTask, "wnSampler", SAMPLER_TASK_STACK, this, SAMPLER_TASK_PRIORITY, NULL);

  return vane_found;
}

// The sampling task replaces the hardware timer tick of the WindNerd Core
// library: vTaskDelayUntil keeps an exact 10 Hz cadence whatever loop() and the
// uploader task are doing.
void WN_Node::samplerTask(void *arg)
{
  WN_Node *node = static_cast<WN_Node *>(arg);
  TickType_t last_wake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(1000 / TICK_HZ);

  for (;;)
  {
    vTaskDelayUntil(&last_wake, period);
    node->tick();
  }
}

void WN_Node::tick()
{
  _ticks_cnt++;

  float angle_deg;
  if (_vane.readAngle(&angle_deg))
  {
    angle_deg = fmodf(angle_deg + _direction_offset + 360.0f, 360.0f);
    _latest_vane_deg = angle_deg;
    // accumulate with an arbitrary magnitude, we are interested only in direction avg
    _vane_averager.accumulate((uint32_t)1, (uint16_t)(angle_deg + 0.5f) % 360);
  }

  if (_ticks_cnt % SAMPLING_WINDOW_TICKS == 0)
  {  // counting window has elapsed

    // check timing, drop the sample if the window was stretched (one or more
    // ticks missed), which would otherwise inflate the measured speed
    bool window_is_valid = (millis() - _last_sampling_window_millis) < (SAMPLE_DURATION * 1000 + 1000 / TICK_HZ);

    if (window_is_valid)
    {
      // average the wind direction over the window and store the data point in
      // the rolling buffer
      wn_raw_wind_report_t vane_raw_report;
      xSemaphoreTake(_buffer_mutex, portMAX_DELAY);
      _vane_averager.computeReportFromAccumulatedValues(&vane_raw_report);
      wn_raw_wind_sample_t raw_sample = {(uint16_t)speed_pulse_count, vane_raw_report.dir_avg, true};

      // reset pulse counter as soon as the sample is recorded
      speed_pulse_count = 0;
      _last_sampling_window_millis = millis();

      _rolling_buffer.addRawSample(raw_sample);
      xSemaphoreGive(_buffer_mutex);

      // callbacks are fired outside the mutex, they are free to read the buffer back
      wn_instant_wind_sample_t sample = formatRawSample(raw_sample, UNIT_MS);
      if (_instant_wind_cb)
        _instant_wind_cb(sample);
    }
    else
    {
      speed_pulse_count = 0;
      _last_sampling_window_millis = millis();
    }
  }

  if (_ticks_cnt % (REPORT_PERIOD_SEC * TICK_HZ) == 0)
  {  // a minute has elapsed
    wn_wind_report_t report = computeReportForPeriodInSecIndexedFromLast(REPORT_PERIOD_SEC, 0, UNIT_MS);
    if (_avg_wind_cb)
      _avg_wind_cb(report);
  }
}

// Compute a wind report for a period (seconds), offset by an index (periods) from the latest data.
wn_wind_report_t WN_Node::computeReportForPeriodInSecIndexedFromLast(uint16_t period, uint16_t index, wn_wind_unit_t unit)
{
  uint16_t samples_to_average = period / SAMPLE_DURATION;
  uint16_t shift = (index * period) / SAMPLE_DURATION;

  // read samples from the rolling buffer and accumulate their cartesian coordinates
  WN_VECTOR_AVERAGER periodAverager;
  xSemaphoreTake(_buffer_mutex, portMAX_DELAY);
  for (uint16_t i = shift; i < samples_to_average + shift; i++)
  {
    wn_raw_wind_sample_t sample = _rolling_buffer.get(i);
    if (sample.valid)
    {
      periodAverager.accumulate(sample);
    }
  }
  xSemaphoreGive(_buffer_mutex);

  // compute 2D averaging, min, max for the period
  wn_raw_wind_report_t avg_raw_wind_report;
  periodAverager.computeReportFromAccumulatedValues(&avg_raw_wind_report);

  return formatRawReport(avg_raw_wind_report, unit);
}

wn_instant_wind_sample_t WN_Node::getSampleIndexedFromLast(uint16_t index, wn_wind_unit_t unit)
{
  xSemaphoreTake(_buffer_mutex, portMAX_DELAY);
  wn_raw_wind_sample_t raw_sample = _rolling_buffer.get(index);
  xSemaphoreGive(_buffer_mutex);
  return formatRawSample(raw_sample, unit);
}

void WN_Node::onInstantWindUpdate(void (*cb)(wn_instant_wind_sample_t sample))
{
  _instant_wind_cb = cb;
}

void WN_Node::onNewWindReport(void (*cb)(wn_wind_report_t report))
{
  _avg_wind_cb = cb;
}

void WN_Node::setFrequencyToWindSpeedRatio(float ratio)
{
  _HZ_to_ms = ratio;
}

void WN_Node::setDirectionOffset(float offset_deg)
{
  _direction_offset = offset_deg;
}

wn_instant_wind_sample_t WN_Node::formatRawSample(wn_raw_wind_sample_t &raw_sample, wn_wind_unit_t unit)
{
  wn_instant_wind_sample_t sample;
  sample.speed = pulsesToSpeed(raw_sample.pulses, unit);
  sample.dir = raw_sample.dir;
  return sample;
}

wn_wind_report_t WN_Node::formatRawReport(wn_raw_wind_report_t &raw_report, wn_wind_unit_t unit)
{
  wn_wind_report_t report;
  report.avg_dir = raw_report.dir_avg;
  report.avg_speed = pulsesToSpeed(raw_report.pulses_avg, unit);
  report.min_speed = pulsesToSpeed(raw_report.pulses_min, unit);
  report.max_speed = pulsesToSpeed(raw_report.pulses_max, unit);
  return report;
}

float WN_Node::pulsesToSpeed(float pulses, wn_wind_unit_t unit)
{
  float speed_ms = pulses * _HZ_to_ms / SAMPLE_DURATION;
  switch (unit)
  {
  case UNIT_MS:
    return speed_ms;
  case UNIT_KN:  // knots
    return speed_ms * 1.94384f;
  case UNIT_KPH:  // kilometers per hour
    return speed_ms * 3.6f;
  case UNIT_MPH:  // miles per hour
    return speed_ms * 2.23694f;
  default:
    return speed_ms;
  }
}
