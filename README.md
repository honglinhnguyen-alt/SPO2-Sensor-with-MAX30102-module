# SPO2 Sensor with MAX30102 module

An Arduino UNO R4 WiFi and MAX30102 pulse-oximeter prototype developed by **Hong Linh Nguyen** for Team 16's astronaut-suit capstone project. The Arduino estimates pulse rate and blood oxygen saturation from optical signals, reports sensor chip temperature, and detects finger contact. The current Wi-Fi version hosts its own local dashboard and JSON API.

**Start with [MAX30102_AP_Web.ino](arduino/MAX30102_AP_Web/MAX30102_AP_Web.ino).** It includes the latest, more sensitive finger-presence thresholds and the offline browser dashboard.

This repository documents a laboratory research prototype. It has not been clinically validated or qualified for spacesuit use. **Chip temperature is not body temperature.** Algorithm validity flags indicate that a calculation passed the library's checks; they do not establish measurement accuracy.

## Contents

- [Features and current status](#features-and-current-status)
- [Choose a sketch](#choose-a-sketch)
- [Hardware and wiring](#hardware-and-wiring)
- [Install and upload](#install-and-upload)
- [Connect a phone or laptop](#connect-a-phone-or-laptop)
- [Serial Monitor and Serial Plotter](#serial-monitor-and-serial-plotter)
- [How the measurement works](#how-the-measurement-works)
- [Sensitivity and configuration](#sensitivity-and-configuration)
- [JSON API](#json-api)
- [Receive readings in another program](#receive-readings-in-another-program)
- [Troubleshooting](#troubleshooting)
- [Verification and next steps](#verification-and-next-steps)
- [References and dependencies](#references-and-dependencies)

## Features and current status

The current access-point sketch provides:

- Pulse-rate estimates in BPM and SpO2 estimates in percent.
- A rolling four-second sample window, recalculated approximately once per second.
- Internal MAX30102 chip-temperature readings.
- Finger-presence detection with separate contact and removal thresholds.
- Measurement reset after contact uncertainty, clipping, or suspected sample loss.
- Full USB Serial Monitor output and SpO2-oriented Serial Plotter formatting.
- A password-protected Arduino Wi-Fi access point.
- An offline dashboard with measurements, warnings, connection status, and approximately two minutes of SpO2 history.
- An HTTP JSON endpoint for teammate software.

The dashboard uses assets stored in the sketch; it does not need an internet connection, cloud account, external chart library, or router. The board still needs a suitable power source.

**Development status:** host-side simulations have passed. The access-point firmware remains experimental: a `Sensor data missing` warning was reported during development, and its hardware root cause has not been confirmed. Lowering presence thresholds does not resolve missing samples. Reference-device accuracy, repeatability, wireless timing, and operation during movement remain to be evaluated.

## Choose a sketch

Upload **one sketch at a time**. Arduino sketch folders and their `.ino` filenames must match.

| File | Purpose | Notes |
|---|---|---|
| [arduino/MAX30102_AP_Web/MAX30102_AP_Web.ino](arduino/MAX30102_AP_Web/MAX30102_AP_Web.ino) | Current Wi-Fi dashboard and JSON API | Recommended starting point; contact thresholds 10,000/5,000 IR counts, two-sample confirmation. |
| [arduino/MAX30102_Combined/MAX30102_Combined.ino](arduino/MAX30102_Combined/MAX30102_Combined.ino) | Original combined USB version | BPM, SpO2, chip temperature, presence warnings; preserves the earlier 50,000/40,000 thresholds and three-sample confirmation for comparison. |
| [arduino/MAX30102_Raw_Test/MAX30102_Raw_Test.ino](arduino/MAX30102_Raw_Test/MAX30102_Raw_Test.ino) | Raw Red/IR signal diagnostic | No BPM or SpO2 calculation; useful for separating contact and acquisition problems from algorithm problems. |
| [arduino/MAX30102_SpO2/MAX30102_SpO2.ino](arduino/MAX30102_SpO2/MAX30102_SpO2.ino) | Earlier standalone HR/SpO2 example | Simpler reference example; does not provide the full presence, temperature, or Wi-Fi features. |
| [examples/receive_arduino.py](examples/receive_arduino.py) | Python HTTP receiver | Uses the Python 3 standard library; no additional packages. |
| [tests/run_checks.py](tests/run_checks.py) | Host-side simulated checks | Uses mocked Arduino interfaces, not a physical sensor or radio. |

## Hardware and wiring

Required hardware:

- Arduino UNO R4 WiFi.
- MAX30102 breakout module with a known power and I2C interface specification.
- USB data cable for programming; suitable power for standalone operation.
- Wiring or connectors for power, ground, SDA, and SCL.
- Phone or laptop with Wi-Fi and a web browser for the wireless version.

The sketches use the standard `Wire` bus and the sensor's usual 7-bit I2C address, `0x57`.

| MAX30102 breakout connection | UNO R4 WiFi connection |
|---|---|
| SDA | Header SDA, also available as A4 |
| SCL | Header SCL, also available as A5 |
| GND | GND |
| VIN/VCC | Supply permitted by the **specific breakout module's** documentation |
| INT, if present | Not required by these polling sketches |

**Confirm power and logic compatibility before connecting a different module.** Breakout VIN ratings are not the bare sensor's VDD rating. The bare MAX30102 uses a nominal 1.8 V VDD supply and a separate LED supply; do not apply a breakout's advertised VIN directly to a bare sensor VDD pin. Verify pull-ups and any level translation in the actual breakout. See the [MAX30102 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max30102.pdf).

The UNO R4 WiFi's Qwiic connector is on a different bus, **`Wire1`**. These sketches use **`Wire`**. If moving a compatible sensor to Qwiic, change the bus supplied to `particleSensor.begin()` accordingly. Header SDA/SCL and A4/A5 share the standard bus; switching between those header labels does not require a code change. See [Arduino's board datasheet](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf).

## Install and upload

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software/).
2. In Boards Manager, install **Arduino UNO R4 Boards** and select **Arduino UNO R4 WiFi**. The board package supplies `WiFiS3`.
3. In Library Manager, install **SparkFun MAX3010x Pulse and Proximity Sensor Library**. The sketches use its `MAX30105.h` class for the MAX30102 and its supplied `spo2_algorithm.h` implementation.
4. Download this repository with **Code -> Download ZIP** and extract it, or clone it:

   ```bash
   git clone https://github.com/honglinhnguyen-alt/SPO2-Sensor-with-MAX30102-module.git
   ```

   A private repository requires an authorised GitHub account.

5. Open `arduino/MAX30102_AP_Web/MAX30102_AP_Web.ino` in Arduino IDE.
6. Select the connected board's USB port, click **Verify**, then **Upload**.
7. Open Serial Monitor at **115200 baud**. Reset the board if you need to see the startup messages again.

No separate ESP32 sketch or firmware replacement is part of this project. Use Arduino's normal board and connectivity-firmware maintenance tools if the board reports a Wi-Fi module problem.

## Connect a phone or laptop

After uploading the access-point sketch:

| Setting | Default |
|---|---|
| Wi-Fi network / SSID | `Capstone-Oximeter` |
| Demonstration password | `Capstone16` |
| Dashboard | `http://192.168.4.1/` |
| JSON API | `http://192.168.4.1/api` |
| Web-server port | `80` |

1. Keep the Arduino powered and wait for its network to appear.
2. Join `Capstone-Oximeter` using the demonstration password.
3. If the device says **No internet**, choose to stay connected. Disable automatic switching away from this network if necessary.
4. Enter **`http://192.168.4.1/`** directly in the browser address bar. Use HTTP rather than HTTPS. The actual address is also printed at startup.
5. Rest a finger gently and steadily on the sensor. Allow approximately four seconds after confirmed contact for the first calculation; poor or interrupted samples can make this take longer.

The dashboard shows BPM, SpO2, chip temperature, raw IR level, contact state, and connection status. It clears current measurements after a failed connection and attempts to reconnect automatically. Invalid or unavailable SpO2 values produce gaps in the browser graph. The graph is browser-local history and is lost on a page reload; the Arduino does not store a measurement log.

For another viewing device, connect that device to the same Arduino network and open the same address. Multiple clients share the small server and can affect timing; simultaneous-client performance has not been measured.

The demonstration Wi-Fi password is published here intentionally. Change `AP_PASSWORD` before wider use. This local prototype uses HTTP and provides no separate web login; access to the AP permits reading its endpoint.

## Serial Monitor and Serial Plotter

Use **115200 baud**. Serial Monitor output resembles the following **illustrative** line:

```text
BPM: 75 | SpO2:97 % | Chip temp: 25.50 C | IR: 60000
```

With no finger, the output resembles:

```text
BPM: -- | SpO2:-- | Chip temp: 25.50 C | IR: 1200 | WARNING: No finger/presence detected
```

The compact `SpO2:97` label targets Arduino IDE 2's labelled-value Plotter parsing. Other monitor labels retain a space after the colon. Both views consume the same serial stream; there is no second serial channel dedicated to the plotter. Parser behaviour should be checked in the IDE version actually used.

For raw-signal troubleshooting, upload `MAX30102_Raw_Test.ino` instead. It outputs `Red:` and `IR:` values. Set `PLOT_IR_ONLY = true` if the channels' different baselines hide the small pulse variation. Raw ADC counts are not BPM or SpO2 percentages.

## How the measurement works

The MAX30102 illuminates tissue with red and infrared light and detects returned light. Changes in arterial blood volume produce a small pulsatile component in the optical signal. The Arduino runs the supplied Maxim algorithm to estimate pulse rate from pulse timing and SpO2 from the red/IR signal relationship. See the [sensor documentation](https://www.analog.com/en/products/max30102.html) and [algorithm source](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/src/spo2_algorithm.cpp).

The optical configuration is:

```cpp
particleSensor.setup(60, 4, 2, 100, 411, 4096);
```

| Parameter | Value | Meaning |
|---|---:|---|
| LED brightness code | 60 | Library LED-drive setting; not a percentage |
| Sample averaging | 4 | Four measurements averaged per FIFO output |
| LED mode | 2 | Red and infrared channels |
| Configured sample rate | 100 samples/s | Before FIFO averaging/decimation |
| Pulse width | 411 microseconds | 18-bit ADC configuration |
| ADC range | 4096 nA | Photodetector current range |

This produces approximately **25 paired FIFO outputs per second**. Each calculation uses **100 red/IR pairs**, equivalent to approximately four seconds. The code retains 75 pairs and adds 25, providing approximately one new calculation each second. Changing the effective rate without changing the algorithm's assumptions can invalidate the timing calculations.

Temperature conversion is started approximately every two seconds and checked on later loop iterations. It is an auxiliary chip measurement and is not used to compensate the current SpO2 calculation.

The web server reads requests and sends responses incrementally. Sensor polling occurs between web operations. `WiFiS3` modem calls can still block. The sketch discards a window after a long service gap or a software-buffer backlog that may have lost samples; it then collects fresh data. This protects the algorithm's sample-timing assumption but may delay or prevent estimates if the hardware repeatedly stalls.

## Sensitivity and configuration

The current AP sketch contains:

```cpp
const uint32_t FINGER_ON_IR = 10000;
const uint32_t FINGER_OFF_IR = 5000;
const uint8_t PRESENCE_CONFIRM_SAMPLES = 2;
```

The code starts recognising contact at or above 10,000 IR counts and retains confirmed contact down to 5,000 counts. Two consecutive contrary samples confirm a state change. Contact uncertainty resets the measurement window immediately, so measurement output may clear before the contact indicator changes.

These are **raw-count presence thresholds**, not a calibrated sensor sensitivity or an accuracy setting. They replace the AP sketch's earlier 50,000/40,000 thresholds and three-sample confirmation. The USB baseline remains available with its earlier settings.

To tune for the actual module:

1. Record uncovered IR values under the intended lighting.
2. Record values with a gently placed, steady finger.
3. Choose an ON threshold that separates those cases; keep OFF below ON and above the uncovered baseline where practical.
4. Confirm that the sensor detects removal and requires a fresh acquisition after contact returns.

If covered and uncovered values overlap, investigate contact, light shielding, saturation, power, and signal quality before choosing another threshold. Increasing LED drive is a separate change and can cause clipping. Values near the 18-bit maximum of **262143** require checking the optical settings; the code rejects values at or above 262140.

Other settings include `AP_SSID`, `AP_PASSWORD`, the AP IP address, serial printing interval, and temperature interval. Changing timing limits or removing sample-loss checks needs actual acquisition testing; simply suppressing warnings does not repair the data stream.

## JSON API

Request the latest snapshot with:

```http
GET /api HTTP/1.1
Host: 192.168.4.1
```

The response is `application/json` with `Cache-Control: no-store`. Example values below are **synthetic**, not test evidence:

```json
{
  "uptime_ms": 12345,
  "sequence": 9,
  "status": "ok",
  "message": "Live estimates received. Chip temperature is not body temperature.",
  "bpm": 75,
  "spo2": 97,
  "chip_temperature_c": 25.5,
  "ir": 60000,
  "finger_present": true,
  "samples_collected": 85,
  "sample_target": 100,
  "sample_age_ms": 12,
  "measurement_age_ms": 400,
  "sampling_resets": 0
}
```

| Field | Type | Meaning |
|---|---|---|
| `bpm` | Number or `null` | Accepted pulse-rate estimate; unavailable values are null. |
| `spo2` | Number or `null` | Accepted SpO2 estimate in percent. |
| `chip_temperature_c` | Number or `null` | Internal sensor temperature in degrees Celsius. |
| `ir` | Number or `null` | Latest raw IR ADC count, if the sample stream is sufficiently recent. |
| `finger_present` | Boolean | Confirmed presence state with a recent stream; this is not a pulse-quality score. |
| `status` | String | Current measurement/acquisition state. |
| `message` | String | Human-readable explanation. |
| `sequence` | Integer | Increments for each completed calculation, even if an algorithm validity flag is false. Resets on board restart. |
| `uptime_ms` | Integer | Milliseconds since board startup. |
| `samples_collected` | Integer | Current buffer fill; returns to 75 after each calculation. |
| `sample_target` | Integer | Initial window size, currently 100 pairs. |
| `sample_age_ms` | Integer | Time since the last pair was retrieved from the library. |
| `measurement_age_ms` | Integer or `null` | Age of the current calculation; null when no calculation is ready. |
| `sampling_resets` | Integer | Count of detected service-gap/backlog resets since startup. |

| Status | Meaning |
|---|---|
| `ok` | Both algorithm validity flags passed for the current calculation. |
| `no_finger` | Finger contact is absent. |
| `collecting` | Waiting for enough continuous samples or a sufficiently recent stream. |
| `invalid` | The algorithm rejected one or both measurements. A separately valid field can still be present. |
| `clipped` | A channel reached the ADC-clipping threshold. |
| `sensor_missing` | Sensor initialisation failed. |
| `data_missing` | No sample has been retrieved for at least one second. |
| `sampling_gap` | A possible sample-loss event caused the window to be reacquired. |

Use **`null` as unavailable**, never as zero or a reason to retain the previous value. Preserve per-field validity: one field can be null while another is usable. Clear current values or mark them stale when the HTTP request fails. Use `sequence` to avoid treating repeated snapshots as new calculations, and handle a board restart when tracking sequences.

The dashboard is `GET /`. The sketch does **not** implement `POST /telemetry`; unsupported methods or paths receive errors.

## Receive readings in another program

Connect the computer to the Arduino AP, then run:

```bash
python examples/receive_arduino.py
```

On Windows, `py` can be used instead of `python`; on some Linux/macOS systems use `python3`.

To retrieve one formatted snapshot:

```bash
python examples/receive_arduino.py --once --pretty
```

To configure polling:

```bash
python examples/receive_arduino.py --url http://192.168.4.1/api --interval 1 --timeout 2
```

The receiver prints one JSON record per poll by default and adds `transport_connected` and `received_at_utc`. On a failed request, malformed response, or invalid measurement type, it emits a `disconnected` record with current measurement fields set to null. It retries on subsequent polls. `--once` exits with status 1 on failure. Press Ctrl+C to stop continuous polling.

Integrate the dictionary returned by `fetch_readings()` with the teammate's dashboard or HUD; run polling outside a GUI's main event loop. A successful HTTP request means that the transport works, not that the sensor measurement is valid. Always inspect `status` and individual fields.

### Connecting to the team's telemetry backend

The related [capstone-astronaut-suit backend](https://github.com/raizellen/capstone-astronaut-suit) currently accepts HTTP **POST** requests at `/telemetry`. This Arduino sketch instead serves HTTP **GET** requests at `/api`. The two interfaces do not connect automatically.

The teammate can add a polling task to their backend, or write a separate bridge that fetches the Arduino API and posts appropriately mapped data to their backend.

| Arduino field | Team backend field | Integration requirement |
|---|---|---|
| `bpm` | `bpm` | Preserve unavailable values and freshness. |
| `spo2` | `spo2` | Preserve unavailable values and freshness. |
| `chip_temperature_c` | `temperature` | Continue to label this as chip temperature. |
| `finger_present`, `status` | No matching field in the reviewed backend | Extend backend/UI handling to represent missing contact and faults. |
| No battery measurement | `battery` | Do not invent or forward a demonstration battery value. |

The reviewed backend ignores non-numeric values, including JSON null, and keeps its previous readings. It therefore needs explicit missing/stale-value handling before a bridge can correctly represent removal or disconnects. The included Python receiver prints state; it does not post to or modify that backend. A frontend hosted on another origin should obtain readings through its backend, or require a deliberate CORS design; this firmware does not provide cross-origin browser access headers.

## Troubleshooting

| Symptom | Meaning / first checks |
|---|---|
| `MAX30102 not found` at startup | Check the actual module's supply, common ground, SDA/SCL, chosen `Wire` bus, and I2C connectivity. Reset after correcting the wiring. |
| `WARNING: Sensor data missing` | Sensor initialisation failed, or no sample pair was retrieved for at least one second. This differs from no finger. Compare operation with the web page closed, then open it to check whether Wi-Fi activity is associated with the interruption. |
| `No finger/presence detected` despite contact | Compare raw covered/uncovered IR counts and adjust the presence thresholds; also check physical contact and illumination. |
| Repeated sampling interruptions / no completed estimate | Inspect `sampling_resets`. Blocking modem calls or another acquisition delay may repeatedly discard the window. Use the USB baseline/raw sketch to isolate networking from optical acquisition. |
| `Signal clipped` / counts near 262143 | Inspect raw waveforms. Consider lower LED drive or a larger supported ADC range, then repeat acquisition checks. Do not increase LED current blindly. |
| BPM or SpO2 fluctuates | Support the hand, use gentle steady contact, shield changing ambient light, inspect raw pulses, and compare against a reference instrument. |
| Wi-Fi network does not appear | Check power and startup Wi-Fi errors in Serial Monitor. Confirm UNO R4 WiFi board selection and board connectivity firmware. |
| Connected to AP but page unavailable | Stay on the no-internet network, use the explicit HTTP address, and check the startup IP. Close extra clients while diagnosing timing. |
| Temperature differs from body temperature | This is expected: the sensor measures its own internal die temperature. |
| Teammate sees old readings after removal | Clear null or stale values in their UI/backend. Do not retain old numeric values when transport or contact is lost. |

## Verification and next steps

### Host-side checks

The optional test suite needs Python 3, a host `g++` compiler, and Node.js:

```bash
python tests/run_checks.py
```

It checks the following using synthetic data and mocked interfaces:

- Current low-IR presence thresholds, hysteresis, removal, and fresh-window reacquisition.
- Rolling sample windows, algorithm validity handling, clipping, temperature state, and missing samples.
- HTTP routing, partial writes, header limits, timeouts, and AP-failure handling.
- Dashboard JavaScript updates, graph gaps, duplicate calculations, disconnect/reconnect, and board restart.
- Python receiver field handling, invalid data, and clearing measurements on disconnect.

The C++ harness includes the actual AP sketch but substitutes small headers from `tests/native`. Those headers are **test doubles**, not replacements for Arduino or SparkFun libraries and must never be copied into an Arduino sketch folder. Passing these checks is not an Arduino board build, a physical Wi-Fi test, optical validation, or evidence of clinical accuracy.

### Hardware checks still needed

1. Verify the selected sketch in Arduino IDE and upload to an UNO R4 WiFi.
2. Check uncovered, steady-contact, movement, and removal behaviour using the real module.
3. Confirm an estimate requires a fresh window after removal or a sample-loss event.
4. Test acquisition with the dashboard closed/open and with the Python receiver connected; record sample interruptions and response timing.
5. Compare results with a reference pulse oximeter under controlled conditions and quantify error, availability, and repeatability.

Future work includes more reliable sensor attachment, signal-quality checks, persistent logging, a clear teammate-backend validity/staleness contract, and suit/HUD integration. Vacuum, radiation, microgravity, automatic oxygen control, and environmental qualification are outside the demonstrated implementation.

## References and dependencies

- [Analog Devices MAX30102 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max30102.pdf): sensor operation, electrical requirements, FIFO/ADC settings, temperature registers.
- [Arduino UNO R4 WiFi documentation](https://docs.arduino.cc/hardware/uno-r4-wifi/) and [board datasheet](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf): board setup and I2C buses.
- [Arduino WiFiS3 access-point example](https://github.com/arduino/ArduinoCore-renesas/blob/main/libraries/WiFiS3/examples/AP_SimpleWebServer/AP_SimpleWebServer.ino): AP/web-server API reference.
- [SparkFun MAX3010x library](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library), [SpO2 example](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/examples/Example8_SPO2/Example8_SPO2.ino), and [algorithm](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/blob/master/src/spo2_algorithm.cpp): sensor driver and supplied calculation.
- [Last Minute Engineers MAX30102 tutorial](https://lastminuteengineers.com/max30102-pulse-oximeter-heart-rate-sensor-arduino-tutorial/): original tutorial context for the example functions combined during development.

Dependencies are installed separately and are not vendored. Their original licences continue to apply. No project-wide open-source licence has been selected for this repository. Library/board versions have not yet been pinned to a hardware-validated release.
