/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Vector_Averager.h"

WN_VECTOR_AVERAGER::WN_VECTOR_AVERAGER()
{
}

void WN_VECTOR_AVERAGER::accumulate(wn_raw_wind_sample_t sample)
{
  accumulate(sample.pulses, sample.dir);
}

void WN_VECTOR_AVERAGER::accumulate(uint32_t pulses, uint16_t dir)
{
  // Convert dir (degrees) into radians
  float rad = dir * (M_PI / 180.0f);

  // Add to vector components (weighted by pulses = speed proxy) for direction
  x += pulses * cosf(rad);
  y += pulses * sinf(rad);

  // Scalar sum of the speed proxy, used for the speed average
  pulses_sum += pulses;

  // Track counts and min/max
  cnt++;
  if (pulses > wind_max)
    wind_max = pulses;
  if (pulses < wind_min)
    wind_min = pulses;
}

void WN_VECTOR_AVERAGER::computeReportFromAccumulatedValues(wn_raw_wind_report_t *report)
{
  if (cnt == 0)
  {
    return;
  }

  // Vector average for the direction only (magnitude is discarded)
  float dir = atan2f(y, x) * 180.0f / M_PI;
  if (dir < 0)
  {
    dir += 360.0f;
  }

  // Scalar average for the speed
  report->pulses_avg = pulses_sum / cnt;
  report->dir_avg = (uint16_t)(dir + 0.5f) % 360;
  report->pulses_max = wind_max;
  report->pulses_min = wind_min;
  x = 0;
  y = 0;
  pulses_sum = 0;
  cnt = 0;
  wind_max = 0;
  wind_min = 0xFFFFFFFF;
}
