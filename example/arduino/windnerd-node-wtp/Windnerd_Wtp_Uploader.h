/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include <freertos/queue.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "Windnerd_Node.h"
#include "Windnerd_Wtp_Payload.h"

// Uploads wind data to windnerd.net over the WindNerd Transfer Protocol.
//
// The upload runs in its own FreeRTOS task so that a slow or failing connection
// never delays the 10 Hz sampling. The task is fed by the two WN_Node
// callbacks: queueSample() every 3 seconds, reportDue() every minute.
//
// One POST goes out every 3 seconds carrying the newest sample, so the live
// animation on windnerd.net is real time. Once a minute the post also carries
// the aggregated report and a log line.
class WN_WTP_UPLOADER
{
public:
  // Starts the uploader task. `secret_key` must stay valid for the lifetime of
  // the program (a string literal in the sketch does).
  void begin(WN_Node *anemometer, const char *secret_key);

  // Hands the newest 3-second sample to the uploader. Never blocks: if the
  // uploader is busy, the oldest queued sample is dropped.
  void queueSample(wn_instant_wind_sample_t sample);

  // Tells the uploader that another minute of reports is waiting to be sent.
  void reportDue();

  // Enables verbose Serial logging of every post.
  void setDebug(Print *debug) { _debug = debug; }

private:
  static void uploaderTask(void *arg);
  void post(wn_instant_wind_sample_t &sample);
  bool connect();

  QueueHandle_t _queue = nullptr;
  portMUX_TYPE _counter_mux = portMUX_INITIALIZER_UNLOCKED;
  uint16_t _pending_report_minutes = 0;

  WN_Node *_anemometer = nullptr;
  const char *_secret_key = "";
  Print *_debug = nullptr;

  WiFiClient _client;
  HTTPClient _http;
  bool _http_ready = false;
  WN_WTP_PAYLOAD _payload;
};
