/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include "Windnerd_Node.h"

typedef struct
{
  bool has_temperature = false;
  bool has_humidity = false;
  bool has_pressure = false;
  bool has_voltage = false;
  bool has_rssi = false;
  bool has_temp_in = false;
  bool has_meta = false;
  bool has_sample = false;
} wn_payload_config_t;

// Builds a WindNerd Transfer Protocol payload in its plain text form, as
// documented at https://windnerd.net/docs/wind-transfer-protocol
//
// Same class as the WindNerd Core library, except that the payload is returned
// as a String instead of being streamed to a modem: HTTPClient::POST() wants a
// buffer, and the Node has plenty of RAM for one.
class WN_WTP_PAYLOAD
{
public:
  // Clears the optional fields; the anemometer and the secret key are kept.
  void reset();

  void setAnemometer(WN_Node *anemometer);
  void setSecretKey(const char *secret_key);

  // How many 1-minute reports the payload carries, newest first (0 = none).
  void setPeriodInMinutes(unsigned int period_mn);
  // The 3-second sample the payload carries.
  void setSample(wn_instant_wind_sample_t sample);

  void setTemperature(float temperature);
  void setHumidity(float humidity);
  void setPressure(float pressure);

  void setVoltage(float voltage);
  void setRSSI(float rssi);
  void setInternalTemperature(float temp_in);
  void setMeta(const char *meta);

  String build();

private:
  void appendReportLine(String &payload, unsigned int line_index);
  void appendLogLine(String &payload);
  void appendSampleLine(String &payload);
  bool hasLogData();

  wn_payload_config_t _payload_config;
  WN_Node *_anemometer = nullptr;
  const char *_secret_key = "";
  unsigned int _period_mn = 0;
  wn_instant_wind_sample_t _sample;
  float _temperature;
  float _humidity;
  float _pressure;
  float _voltage;
  float _rssi;
  float _temp_in;
  const char *_meta;
};
