/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Wtp_Payload.h"

#define SPEED_MAX_LENGTH 4  // 99.9 m/s
#define DIR_MAX_LENGTH 4    // 359

#define TEMP_MAX_LENGTH 5      //-99.0 C/F
#define HUM_MAX_LENGTH 3       // 100
#define PRESSURE_MAX_LENGTH 6  // 1099.9
#define VOLTAGE_MAX_LENGTH 4   // 4.30
#define RSSI_MAX_LENGTH 5      // -99.9
#define TEMP_IN_MAX_LENGTH 5   //-99.0

// The constants above are dtostrf field widths, i.e. minimums: a value
// wider than expected is printed in full. Buffers get room for that.
#define NUM_BUFFER_LENGTH 16

void WN_WTP_PAYLOAD::reset()
{
  _payload_config = {};
  _period_mn = 0;
}

void WN_WTP_PAYLOAD::setAnemometer(WN_Node *anemometer)
{
  _anemometer = anemometer;
}

void WN_WTP_PAYLOAD::setSecretKey(const char *secret_key)
{
  _secret_key = secret_key;
}

void WN_WTP_PAYLOAD::setPeriodInMinutes(unsigned int period_mn)
{
  _period_mn = period_mn;
}

void WN_WTP_PAYLOAD::setSample(wn_instant_wind_sample_t sample)
{
  _sample = sample;
  _payload_config.has_sample = true;
}

void WN_WTP_PAYLOAD::setTemperature(float temperature)
{
  _temperature = temperature;
  _payload_config.has_temperature = true;
}

void WN_WTP_PAYLOAD::setHumidity(float humidity)
{
  _humidity = humidity;
  _payload_config.has_humidity = true;
}

void WN_WTP_PAYLOAD::setPressure(float pressure)
{
  _pressure = pressure;
  _payload_config.has_pressure = true;
}

void WN_WTP_PAYLOAD::setVoltage(float voltage)
{
  _voltage = voltage;
  _payload_config.has_voltage = true;
}

void WN_WTP_PAYLOAD::setRSSI(float rssi)
{
  _rssi = rssi;
  _payload_config.has_rssi = true;
}

void WN_WTP_PAYLOAD::setInternalTemperature(float temp_in)
{
  _temp_in = temp_in;
  _payload_config.has_temp_in = true;
}

void WN_WTP_PAYLOAD::setMeta(const char *meta)
{
  _meta = meta;
  _payload_config.has_meta = true;
}

bool WN_WTP_PAYLOAD::hasLogData()
{
  return _payload_config.has_voltage || _payload_config.has_rssi ||
         _payload_config.has_temp_in || _payload_config.has_meta;
}

// one aggregated wind report, indexed backwards from the most recent minute
void WN_WTP_PAYLOAD::appendReportLine(String &payload, unsigned int line_index)
{
  // WTP always transmits m/s, the server's default unit
  wn_wind_report_t report = _anemometer->computeReportForPeriodInSecIndexedFromLast(60, line_index, UNIT_MS);

  char wa[NUM_BUFFER_LENGTH], wn[NUM_BUFFER_LENGTH], wx[NUM_BUFFER_LENGTH], wd[NUM_BUFFER_LENGTH];
  dtostrf(report.avg_speed, SPEED_MAX_LENGTH, 1, wa);
  dtostrf(report.min_speed, SPEED_MAX_LENGTH, 1, wn);
  dtostrf(report.max_speed, SPEED_MAX_LENGTH, 1, wx);
  dtostrf(report.avg_dir, DIR_MAX_LENGTH, 0, wd);

  payload += "r,wa=";
  payload += wa;
  payload += ",wd=";
  payload += wd;
  payload += ",wn=";
  payload += wn;
  payload += ",wx=";
  payload += wx;

  // extra weather data describes the moment of the post, so it rides on the
  // most recent report only
  if (line_index == 0)
  {
    if (_payload_config.has_temperature)
    {
      char temperature[NUM_BUFFER_LENGTH];
      dtostrf(_temperature, TEMP_MAX_LENGTH, 1, temperature);
      payload += ",tp=";
      payload += temperature;
    }
    if (_payload_config.has_humidity)
    {
      char humidity[NUM_BUFFER_LENGTH];
      dtostrf(_humidity, HUM_MAX_LENGTH, 0, humidity);
      payload += ",hu=";
      payload += humidity;
    }
    if (_payload_config.has_pressure)
    {
      char pressure[NUM_BUFFER_LENGTH];
      dtostrf(_pressure, PRESSURE_MAX_LENGTH, 1, pressure);
      payload += ",pr=";
      payload += pressure;
    }
  }

  payload += ";";
}

// device health data
void WN_WTP_PAYLOAD::appendLogLine(String &payload)
{
  payload += "l";

  if (_payload_config.has_voltage)
  {
    char voltage[NUM_BUFFER_LENGTH];
    dtostrf(_voltage, VOLTAGE_MAX_LENGTH, 2, voltage);
    payload += ",vo=";
    payload += voltage;
  }
  if (_payload_config.has_rssi)
  {
    char rssi[NUM_BUFFER_LENGTH];
    dtostrf(_rssi, RSSI_MAX_LENGTH, 1, rssi);
    payload += ",rs=";
    payload += rssi;
  }
  if (_payload_config.has_temp_in)
  {
    char temp_in[NUM_BUFFER_LENGTH];
    dtostrf(_temp_in, TEMP_IN_MAX_LENGTH, 1, temp_in);
    payload += ",ti=";
    payload += temp_in;
  }
  if (_payload_config.has_meta)
  {
    payload += ",mt=";
    payload += _meta;
  }

  payload += ";";
}

// one high frequency wind sample, broadcast in real time by the server
void WN_WTP_PAYLOAD::appendSampleLine(String &payload)
{
  char wi[NUM_BUFFER_LENGTH], wd[NUM_BUFFER_LENGTH];
  dtostrf(_sample.speed, SPEED_MAX_LENGTH, 1, wi);
  dtostrf(_sample.dir, DIR_MAX_LENGTH, 0, wd);

  payload += "s,wi=";
  payload += wi;
  payload += ",wd=";
  payload += wd;
  payload += ";";
}

String WN_WTP_PAYLOAD::build()
{
  String payload;
  payload.reserve(128 + _period_mn * 48);

  // parameters line: the interval is only meaningful when reports are attached
  payload += "k=";
  payload += _secret_key;
  if (_period_mn > 0)
  {
    payload += ",i=1";
  }
  payload += ",wu=ms;";

  // lines must be ordered from most recent to most ancient
  for (unsigned int i = 0; i < _period_mn; i++)
  {
    appendReportLine(payload, i);
  }

  if (hasLogData())
  {
    appendLogLine(payload);
  }

  if (_payload_config.has_sample)
  {
    appendSampleLine(payload);
  }

  return payload;
}
