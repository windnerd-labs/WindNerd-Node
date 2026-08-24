# WindNerd Node → windnerd.net (Arduino)

Alternative firmware for the WindNerd Node that publishes your station on
[windnerd.net](https://windnerd.net) instead of talking to Home Assistant.

It speaks the [WindNerd Transfer Protocol](https://github.com/windnerd-labs/Windnerd-Core/blob/main/docs/WTP.md):
one HTTP post every 3 seconds carries a live wind sample, and once a minute the
post also carries the aggregated report and a device log line.

Flashing this sketch **replaces the ESPHome firmware** the board ships with. You
can always go back: build [`../../esphome/factory.yaml`](../../esphome/factory.yaml)
with ESPHome device builder and upload it over USB.

## What you need

- **Arduino IDE 2.x**
- The **ESP32 board package, version 3.x** ("esp32" by Espressif Systems, in
  Boards Manager).
- No other library. Everything else is in this folder.

Download or copy the whole `windnerd-node-wtp` folder and open
`windnerd-node-wtp.ino` — the Arduino IDE compiles the `.cpp` files next to it
automatically.

## Board settings

| Setting | Value |
|---|---|
| Board | **ESP32C3 Dev Module** |
| USB CDC On Boot | **Enabled** (needed to see the serial monitor over USB) |
| Flash Size | 4 MB |
| Partition Scheme | Default 4MB with spiffs |
| Upload Speed | 921600 |

## Configure

Four lines at the top of the sketch, and nothing else:

```cpp
#define WTP_SECRET_KEY "0000000000000000"  // your station key
#define WIFI_SSID      "my-wifi"
#define WIFI_PASSWORD  "my-password"
```

The **secret key** is the 16 hexadecimal characters shipped with your board. It
is also shown, and can be regenerated, in your
[station management console](https://windnerd.net/en/management).

The Wi-Fi network has to be **2.4 GHz** — the ESP32-C3 has no 5 GHz radio.

## Check it works

Open the serial monitor at **115200 baud**. Within a few seconds of power-up:

```
Wind vane found at I2C address 0x22
Connecting to Wi-Fi network "my-wifi"
WTP: POST 200 reports 0 rssi -58 > k=3122fd880084fd55,wu=ms;s,wi= 3.1,wd=  90;
WTP: POST 200 reports 0 rssi -58 > k=3122fd880084fd55,wu=ms;s,wi= 2.8,wd=  92;
Wind report: avg 3.0 m/s, dir 91 deg, min 0.5, max 4.2
WTP: POST 200 reports 1 rssi -57 > k=3122fd880084fd55,i=1,wu=ms;r,wa= 3.0,wd=  91,wn= 0.5,wx= 4.2;l,rs=-57.0,ti= 41.2,mt=node arduino up 61s heap 187364;s,wi= 3.3,wd=  88;
```

`POST 200` means windnerd.net accepted the data. Comment out
`#define ENABLE_SERIAL_DEBUG` once you are happy, and only refused posts are
reported.

The **onboard LED** behaves as with the shipped firmware. It lights for two
seconds at power-up, so you can tell the board came alive before testing
anything else. After that it checks the sensors for you before the anemometer
is mounted: spin the cups and it flashes once per anemometer pulse, rotate the
vane and it lights solid within 15 degrees of north.


## If something is wrong

| Symptom | Cause |
|---|---|
| `POST 401` | wrong `WTP_SECRET_KEY` |
| `POST 400` | payload refused — please report it to us |
| `WTP: Wi-Fi down, sample dropped` | wrong credentials, or a 5 GHz-only network |
| `No wind vane found on the I2C bus` | the vane sensor did not answer; check nothing else on the extension connector is using address 0x22 |
| Direction is stuck | same as above — speed keeps working without the vane |


## Adding your own sensors

The extension connector (see the [main README](../../../README.md)) brings out
the I²C bus. WTP carries ambient temperature, humidity and pressure, so a BME280
only needs three extra lines in `Windnerd_Wtp_Uploader.cpp`, next to where the
log data is filled in:

```cpp
_payload.setTemperature(bme.getTemperature());
_payload.setHumidity(bme.getHumidity());
_payload.setPressure(bme.getPressure() / 100);
```

## How it is put together

| File | Role |
|---|---|
| `windnerd-node-wtp.ino` | your settings, start-up, diagnostic LED |
| `Windnerd_Node.*` | sampling engine: counts pulses and reads the vane at 10 Hz in its own FreeRTOS task, publishes a sample every 3 s and a report every minute |
| `Windnerd_TMAG5273.*` | wind vane angle sensor driver |
| `Windnerd_I2C.*` | I²C helper with bus recovery |
| `Windnerd_Rolling_Buffer.*` | last 20 minutes of 3-second samples |
| `Windnerd_Vector_Averager.*` | vector averaging — the correct way to average angles |
| `Windnerd_Wtp_Payload.*` | builds the WTP payload |
| `Windnerd_Wtp_Uploader.*` | FreeRTOS task that posts it over HTTP |

Most of this comes from the [WindNerd Core](https://github.com/windnerd-labs/windnerd-core) Arduino library for STM32.
