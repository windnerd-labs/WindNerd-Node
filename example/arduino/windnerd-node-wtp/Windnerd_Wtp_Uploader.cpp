/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Wtp_Uploader.h"
#include <WiFi.h>

// Plain HTTP on purpose. Nothing here is critical and token can be renewed.
// That skips the TLS handshake and the mbedTLS buffers every 3 seconds
#define WTP_URL "http://wtp.windnerd.net/post"
#define WTP_TIMEOUT_MS 10000

// A payload carries at most 20 reports (protocol limit), which is also how far
// back the rolling buffer goes: a Wi-Fi outage shorter than that is fully
// backfilled on the next successful post.
#define MAX_PENDING_REPORT_MINUTES 20

// Depth is small on purpose: if the uploader is stuck, stale samples are worth
// less than fresh ones.
#define SAMPLE_QUEUE_DEPTH 4

// Pause after a refused or failed post, so a wrong key does not turn into a
// reconnection storm. Samples keep flowing into the queue meanwhile.
#define RETRY_BACKOFF_MS 5000

// Room for the HTTP client and the payload building
#define UPLOADER_TASK_STACK 10240
#define UPLOADER_TASK_PRIORITY 2

void WN_WTP_UPLOADER::begin(WN_Node *anemometer, const char *secret_key)
{
  _anemometer = anemometer;
  _secret_key = secret_key;

  _queue = xQueueCreate(SAMPLE_QUEUE_DEPTH, sizeof(wn_instant_wind_sample_t));

  _payload.setAnemometer(_anemometer);
  _payload.setSecretKey(_secret_key);

  _client.setTimeout(WTP_TIMEOUT_MS / 1000);

  xTaskCreate(uploaderTask, "wnUploader", UPLOADER_TASK_STACK, this, UPLOADER_TASK_PRIORITY, NULL);
}

void WN_WTP_UPLOADER::queueSample(wn_instant_wind_sample_t sample)
{
  if (_queue == nullptr)
    return;

  // make room rather than block the sampling task
  if (xQueueSend(_queue, &sample, 0) != pdTRUE)
  {
    wn_instant_wind_sample_t dropped;
    xQueueReceive(_queue, &dropped, 0);
    xQueueSend(_queue, &sample, 0);
  }
}

void WN_WTP_UPLOADER::reportDue()
{
  portENTER_CRITICAL(&_counter_mux);
  if (_pending_report_minutes < MAX_PENDING_REPORT_MINUTES)
  {
    _pending_report_minutes++;
  }
  portEXIT_CRITICAL(&_counter_mux);
}

void WN_WTP_UPLOADER::uploaderTask(void *arg)
{
  WN_WTP_UPLOADER *uploader = static_cast<WN_WTP_UPLOADER *>(arg);

  for (;;)
  {
    wn_instant_wind_sample_t sample;
    if (xQueueReceive(uploader->_queue, &sample, portMAX_DELAY) == pdTRUE)
    {
      uploader->post(sample);
    }
  }
}

bool WN_WTP_UPLOADER::connect()
{
  if (_http_ready)
    return true;

  if (!_http.begin(_client, WTP_URL))
    return false;

  // Keeping the connection open means the TCP handshake is paid once, not every
  // 3 seconds. Headers are set here because addHeader() accumulates.
  _http.setReuse(true);
  _http.setTimeout(WTP_TIMEOUT_MS);
  _http.setConnectTimeout(WTP_TIMEOUT_MS);
  _http.addHeader("Content-Type", "text/plain");
  _http_ready = true;
  return true;
}

void WN_WTP_UPLOADER::post(wn_instant_wind_sample_t &sample)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (_debug)
      _debug->println("WTP: Wi-Fi down, sample dropped");
    return;
  }

  // Take the pending reports now and clear only what was actually sent, so a
  // minute elapsing during the post is not lost.
  portENTER_CRITICAL(&_counter_mux);
  uint16_t minutes_to_send = _pending_report_minutes;
  portEXIT_CRITICAL(&_counter_mux);

  _payload.reset();
  _payload.setSample(sample);
  _payload.setPeriodInMinutes(minutes_to_send);

  char meta[64];
  if (minutes_to_send > 0)
  {
    // health data rides along with the minute report
    _payload.setRSSI(WiFi.RSSI());
    _payload.setInternalTemperature(temperatureRead());
    snprintf(meta, sizeof(meta), "node arduino up %lus heap %lu",
             millis() / 1000,
             (unsigned long)ESP.getFreeHeap());
    _payload.setMeta(meta);
  }

  String body = _payload.build();

  if (!connect())
  {
    if (_debug)
      _debug->println("WTP: connection to wtp.windnerd.net failed");
    return;
  }

  int code = _http.POST(body);

  if (_debug)
  {
    _debug->print("WTP: POST ");
    _debug->print(code);
    _debug->print(" reports ");
    _debug->print(minutes_to_send);
    _debug->print(" rssi ");
    _debug->print(WiFi.RSSI());
    _debug->print(" > ");
    _debug->println(body);
  }

  if (code == HTTP_CODE_OK)
  {
    portENTER_CRITICAL(&_counter_mux);
    _pending_report_minutes -= minutes_to_send;
    portEXIT_CRITICAL(&_counter_mux);
  }
  else
  {
    // 401 means the secret key is wrong, 400 a malformed payload; both are worth
    // reporting even when debug output is off.
    if (!_debug && code > 0)
    {
      Serial.print("WTP: server refused the post, HTTP ");
      Serial.println(code);
    }
    // drop the connection so the next post starts from a clean state
    _http.end();
    _http_ready = false;

    // Back off before trying again: a wrong secret key or a server hiccup
    // should not turn into a reconnection every 3 seconds.
    vTaskDelay(pdMS_TO_TICKS(RETRY_BACKOFF_MS));
  }
}
