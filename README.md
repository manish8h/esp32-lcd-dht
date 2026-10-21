# ESP32 Temperature & Humidity Monitor

Arduino sketch using a **DHT22** temperature/humidity sensor and an **ST7789 SPI TFT display**, with a web dashboard hosted on the ESP32.

Repository: [manish8h/esp32-lcd-dht](https://github.com/manish8h/esp32-lcd-dht)

The project consists of `lcd_and_dht.ino`. This README describes the checked-in sketch and locally installed dependency metadata; it does not establish that a particular board or wiring arrangement has been tested.

## Features

- Reads the DHT22 approximately every 2 seconds after setup.
- Displays Fahrenheit first, Celsius second, relative humidity, Wi-Fi status, IP address, and uptime on the TFT.
- Serves a responsive dashboard with current readings, minimum/maximum values, history charts, uptime, and sensor error count.
- Keeps up to 1,440 paired temperature/humidity samples in a RAM ring buffer, with a sampling opportunity every 60 seconds. The oldest sample is overwritten when full.
- Continues local display monitoring if the initial Wi-Fi connection fails, and attempts reconnection in the background.
- Prints readings, Wi-Fi events, and periodic diagnostics to the serial port.

## Hardware

The sketch explicitly configures:

| Component | Configuration in the sketch |
| --- | --- |
| Microcontroller | ESP32 using the Arduino ESP32 core; exact board model is not recorded |
| Sensor | DHT22 (`DHTTYPE DHT22`) |
| Display | ST7789 over SPI, initialized as 240 × 240 pixels, rotation `0` |
| Network | Wi-Fi station mode, connecting to an existing router |

The source does not identify the TFT's physical size, module manufacturer, ESP32 board variant, supply connections, sensor pull-up arrangement, or backlight wiring. Confirm these against the actual modules before powering the circuit. No I2C character LCD is configured.

## Signal wiring

Numbers below are **ESP32 GPIO numbers**, not physical header positions.

| Peripheral signal | ESP32 GPIO | Source setting |
| --- | --- | --- |
| DHT22 data | 27 | `DHTPIN` |
| TFT chip select (CS) | 5 | `TFT_CS` |
| TFT data/command (DC) | 16 | `TFT_DC` |
| TFT reset (RST) | 4 | `TFT_RST` |
| TFT SPI data input (MOSI) | 23 | `TFT_MOSI` |
| TFT SPI clock (SCLK) | 18 | `TFT_SCLK` |
| TFT MISO | Not configured | `SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS)` |

The TFT uses hardware SPI through `Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST)`.

Connect the modules to a common ground. Determine VCC, backlight/BL, and any DHT data pull-up connections from the documentation for your exact hardware; the sketch does not specify voltages or control a backlight GPIO. Confirm that the selected board exposes these GPIOs and that they are available for this circuit.

## Dependencies

Install the **esp32** board package from Espressif Systems and the following libraries through the Arduino IDE's library manager. Accept the library manager's dependency installation prompts.

| Library/package | Purpose | Version found locally when documenting |
| --- | --- | --- |
| esp32 board package | ESP32 support; supplies `WiFi.h`, `WebServer.h`, and `SPI.h` | 3.3.12 |
| DHT sensor library by Adafruit | `DHT.h`, DHT22 readings | 1.4.7 |
| Adafruit GFX Library | `Adafruit_GFX.h`, display graphics/text | 1.12.6 |
| Adafruit ST7735 and ST7789 Library | `Adafruit_ST7789.h`, display driver | 1.11.0 |
| Adafruit Unified Sensor | Declared dependency of the DHT library | 1.1.15 |
| Adafruit BusIO | Declared dependency of Adafruit GFX | 1.17.4 |

The installed ST7735/ST7789 library also declares dependencies on Adafruit GFX Library, Adafruit seesaw Library, and SD. The sketch does not itself use an SD card or seesaw peripheral.

These are observed local versions, not pinned requirements or a verified compatibility matrix. No board selection, dependency lockfile, or build configuration is committed. The dashboard's HTML, CSS, JavaScript, and canvas chart drawing are embedded in the sketch; there is no separate web build or external chart library.

## Setup and upload

1. Obtain the project, if you do not already have it:

   ```sh
   git clone https://github.com/manish8h/esp32-lcd-dht.git lcd_and_dht
   cd lcd_and_dht
   ```

   The destination folder name matches `lcd_and_dht.ino` for Arduino IDE sketch naming. The existing local checkout is `/Users/manish/Documents/Arduino/lcd_and_dht`.

2. Open `lcd_and_dht.ino` in the Arduino IDE and install the dependencies above.
3. Select the actual ESP32 board model and its connected serial port. The repository does not establish an exact board selection.
4. Edit `ssid` and `password` in the sketch's **WIFI SETTINGS** section for your network. These are currently stored as string literals; the project has no separate secrets/configuration file. Keep real credentials out of published changes.
5. With power disconnected, connect the signal pins listed above and confirm the module-specific power, logic-level, pull-up, and backlight requirements.
6. Connect the ESP32, verify/compile the sketch, and upload it.
7. Open the Serial Monitor at **115200 baud**. On successful connection, look for `ESP32 IP Address:` or `IP Address:`; the TFT also displays the IP.
8. From a browser on a network that can reach the ESP32, open `http://<ESP32-IP>/`, substituting the device's actual IP address.

The initial Wi-Fi connection wait lasts up to approximately 15 seconds. Sensor monitoring begins after that wait, whether or not Wi-Fi connected. There is no access-point setup portal, fixed IP, or mDNS hostname configured. The server uses HTTP on port 80 and has no authentication or TLS configured.

## Usage and behavior

- **TFT:** Shows the `SMART HOME` layout, temperature in °F and °C, humidity in percent, network state, and uptime in days/hours/minutes. Changing regions are redrawn as needed; the main loop calls the display update after each sensor read.
- **Dashboard:** Fetches current data immediately and every 2 seconds. Fetches history immediately, every 120 seconds, and after a debounced window resize. Charts show temperature in °F and humidity in percent.
- **Minimum/maximum:** Computed from every successful sensor read since boot, not just the retained history window. The dashboard shows temperature extrema in °F; the JSON API supplies °C.
- **History:** Attempts an initial sample after the first sensor read, then samples every 60 seconds. Invalid sensor state skips recording. A full buffer represents roughly 24 hours of successful minute samples under normal operation; failures can make that span longer. Samples have no timestamps. Chart spacing and the displayed “History: N minutes” label assume one sample per minute, so gaps are not represented accurately.
- **Reset/power loss:** Clears all readings, extrema, history, uptime, and error counts. There is no persistent storage or cloud upload.
- **Sensor failure:** A failed temperature or humidity read increments the error count, marks the sensor invalid, logs `DHT22 Read Error`, and makes current API readings `null`. The TFT shows `Sensor Error`; previous extrema and recorded history remain available.
- **Wi-Fi recovery:** Automatic reconnect is enabled. After 15 seconds disconnected, the sketch requests a manual reconnect, with further requests at least 60 seconds apart while disconnected. Sensor and TFT updates continue.
- **Diagnostics:** A roughly 60-second serial heartbeat reports uptime, Wi-Fi state, free heap, and Wi-Fi status code; when connected it also reports IP and RSSI.

## HTTP endpoints

| Path | Response |
| --- | --- |
| `/` | Embedded HTML dashboard |
| `/data` | JSON current readings, extrema, validity, uptime, and error count |
| `/history` | Streamed JSON history, oldest sample first |

`/data` fields:

| Field | Meaning |
| --- | --- |
| `temperature` | Current temperature in °C; `null` when sensor invalid |
| `humidity` | Current relative humidity in percent; `null` when sensor invalid |
| `minTemp`, `maxTemp` | Temperature extrema in °C since boot; `null` until a valid reading |
| `minHumidity`, `maxHumidity` | Humidity extrema in percent since boot; `null` until a valid reading |
| `sensorValid` | Whether the latest read succeeded |
| `uptime` | `millis() / 1000`, in seconds |
| `errors` | Number of failed sensor read attempts since boot |

`/history` returns `intervalSeconds` set to `60` and a `samples` array. Each entry is `[temperatureC, humidityPercent]`, rounded to one decimal place. The array is empty until a valid sample has been recorded. Temperature remains in °C in the API; the browser converts it to °F for the chart.

## Troubleshooting

- **`DHT22 Read Error` / `Sensor Error`:** Check sensor type, data connection to GPIO 27, module power/ground, and the module's pull-up requirements. A DHT11 requires changing the configured sensor type; it is not what this sketch specifies.
- **Blank or incorrect TFT output:** Check that the module is ST7789-compatible, its expected dimensions/rotation, all SPI/control wires, and its power/backlight requirements. No backlight pin is driven in this code.
- **No dashboard:** Check the configured Wi-Fi credentials and the IP shown on the TFT/Serial Monitor. The browser must be able to reach the device; router client isolation can prevent this. Use HTTP, port 80.
- **History initially empty:** Wait for a successful sensor read and a history sampling opportunity. Browser history refreshes every two minutes. If charts fail, check the browser console; the code uses `fetch`, canvas, and `AbortSignal.timeout(10000)`.
- **Missing-header errors:** Install the library matching the header and its dependencies, and ensure an ESP32 board is selected.

## GitHub remote

The existing checkout's `origin` is:

```text
git@github-personal:manish8h/esp32-lcd-dht.git
```

`github-personal` is a machine-specific SSH host alias. Other machines need their own equivalent SSH configuration or can use the HTTPS clone URL above. The inspected checkout is on `main`.
