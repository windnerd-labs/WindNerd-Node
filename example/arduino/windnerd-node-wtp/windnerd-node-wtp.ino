/*
 * WindNerd Node -> windnerd.net
 *
 * Publishes wind speed and direction on windnerd.net using the WindNerd
 * Transfer Protocol: https://windnerd.net/docs/wind-transfer-protocol
 *
 * One post every 3 seconds carries a live wind sample, and once a minute it
 * also carries the aggregated report. The upload runs in its own FreeRTOS task,
 * so the wind sampling keeps its exact timing whatever the network is doing.
 *
 * Board: WindNerd Node (ESP32-C3). See README.md for the IDE settings.
 *
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Arduino.h"
#include <WiFi.h>
#include "Windnerd_Node.h"
#include "Windnerd_Wtp_Uploader.h"

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "This sketch needs version 3.x of the ESP32 Arduino core (Boards Manager: \"esp32\" by Espressif Systems)."
#endif

// ─── Edit these four lines ──────────────────────────────────────────────────

// The secret key of your station, printed on the label that came with the
// board, and available in your console at https://windnerd.net/en/management
#define WTP_SECRET_KEY "YOUR_STATION_SECRET_KEY"

// Your 2.4 GHz Wi-Fi network
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Degrees added to the raw vane angle so that 0 deg = true north.
// Adjust after mounting the anemometer.
#define DIRECTION_OFFSET 0.0f

// ────────────────────────────────────────────────────────────────────────────

// Comment out to stop printing every post on the serial monitor.
#define ENABLE_SERIAL_DEBUG

WN_Node Anemometer;
WN_WTP_UPLOADER Uploader;

// Diagnostic LED, same behaviour as the ESPHome firmware the board ships with:
//   at power-up = lit for BOOT_FLASH_MS, the firmware is alive
//   solid on    = vane within +/- NORTH_WINDOW_DEG of north, for aligning the vane
//   flash       = one anemometer pulse, PULSE_FLASH_MS long, no averaging
#define LED_PWM_FREQUENCY 1000
#define LED_PWM_RESOLUTION 8
#define NORTH_WINDOW_DEG 15.0f
#define PULSE_FLASH_MS 40
#define BOOT_FLASH_MS 2000
// Poll fast enough to catch the reed contact, which is far shorter than a turn.
#define LED_TICK_MS 2

// Called from the sampling task every 3 seconds.
void instantWindCallback(wn_instant_wind_sample_t sample) {
  Uploader.queueSample(sample);
}

// Called from the sampling task every minute.
void windReportCallback(wn_wind_report_t report) {
  Uploader.reportDue();
#ifdef ENABLE_SERIAL_DEBUG
  Serial.printf("Wind report: avg %.1f m/s, dir %u deg, min %.1f, max %.1f\n",
                report.avg_speed, (unsigned)report.avg_dir, report.min_speed, report.max_speed);
#endif
}

void setup() {
  Serial.begin(115200);

  ledcAttach(NODE_LED_PIN, LED_PWM_FREQUENCY, LED_PWM_RESOLUTION);

  // Alive indicator: light the LED now, and loop() holds it lit until
  // BOOT_FLASH_MS have passed. Lighting it here rather than waiting means the
  // rest of setup happens behind the indication instead of after it, so
  // nothing is delayed by the two seconds.
  ledcWrite(NODE_LED_PIN, (1 << LED_PWM_RESOLUTION) - 1);

  Anemometer.setDirectionOffset(DIRECTION_OFFSET);
  Anemometer.onInstantWindUpdate(&instantWindCallback);
  Anemometer.onNewWindReport(&windReportCallback);

  if (Anemometer.begin()) {
    Serial.printf("Wind vane found at I2C address 0x%02X\n", Anemometer.getVaneAddress());
  } else {
    Serial.println("No wind vane found on the I2C bus, direction will read 0");
  }

  Uploader.begin(&Anemometer, WTP_SECRET_KEY);
#ifdef ENABLE_SERIAL_DEBUG
  Uploader.setDebug(&Serial);
#endif

  // Wi-Fi comes up in the background: sampling has already started, and the
  // uploader simply drops samples until the network is there.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to Wi-Fi network \"%s\"\n", WIFI_SSID);
}

void loop() {
  // The sampling library owns this pin's interrupt, so the LED just watches the
  // line for the same rising edge rather than attaching a second handler.
  static bool pulse_pin_was_high = digitalRead(NODE_SPEED_INPUT_PIN) == HIGH;
  static uint32_t flash_until = 0;

  const bool pulse_pin_high = digitalRead(NODE_SPEED_INPUT_PIN) == HIGH;
  if (pulse_pin_high && !pulse_pin_was_high) {
    flash_until = millis() + PULSE_FLASH_MS;
  }
  pulse_pin_was_high = pulse_pin_high;

  // Angular distance from north, 0..180, with no jump at the 360 -> 0 wrap.
  // Straight off the newest 10 Hz vane reading, so the window tracks the vane.
  const float dir = Anemometer.getLatestVaneAngle();
  const float off_north = fabsf(fmodf(dir + 180.0f, 360.0f) - 180.0f);

  // Near north the LED is lit anyway, so the flashes simply do not show there.
  // A vane that never answered leaves off_north NAN, so the comparison is false.
  // millis() is counted from boot, so the first BOOT_FLASH_MS are the alive
  // indication started in setup().
  const bool lit = millis() < BOOT_FLASH_MS ||
                   (int32_t)(millis() - flash_until) < 0 ||
                   off_north <= NORTH_WINDOW_DEG;
  ledcWrite(NODE_LED_PIN, lit ? (1 << LED_PWM_RESOLUTION) - 1 : 0);

  delay(LED_TICK_MS);
}
