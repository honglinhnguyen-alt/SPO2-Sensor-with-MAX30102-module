/*
  MAX30102 + Arduino Uno R4 WiFi: BPM, SpO2, die temperature and presence.
  Install SparkFun MAX3010x Pulse and Proximity Sensor Library.
  Serial Monitor and Serial Plotter (Arduino IDE 2): 115200 baud.
  Monitor shows BPM, SpO2, chip temperature, IR and warnings.
  Plotter shows only SpO2 from the same output stream.
  Uses the board's default Wire SDA/SCL pins.
  BPM and SpO2 use the same Maxim algorithm supplied in spo2_algorithm.h.
  Temperature is the sensor chip's temperature, not body temperature.
  Presence thresholds are starting values; adjust using the printed IR count.
  Keep SpO2:value without a space after the colon for IDE 2 plotting.
  Other labels deliberately have a space after the colon, so the IDE 2
  plotter does not treat them as label-value pairs. Unavailable SpO2 is --.
*/

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

const uint16_t SAMPLE_COUNT = 100;
const uint16_t UPDATE_COUNT = 25;
const uint32_t FINGER_ON_IR = 50000;
const uint32_t FINGER_OFF_IR = 40000;
const uint8_t PRESENCE_CONFIRM_SAMPLES = 3;
const unsigned long PRINT_INTERVAL_MS = 1000;
const unsigned long TEMP_INTERVAL_MS = 2000;
const unsigned long TEMP_TIMEOUT_MS = 100;
const unsigned long DATA_TIMEOUT_MS = 1000;

// Uno R4 uses 32-bit sample buffers.
uint32_t redBuffer[SAMPLE_COUNT];
uint32_t irBuffer[SAMPLE_COUNT];
uint16_t samplesStored = 0;

int32_t heartRate = 0;
int32_t spo2 = 0;
int8_t validHeartRate = 0;
int8_t validSPO2 = 0;
bool calculationReady = false;
bool fingerPresent = false;
bool signalClipped = false;
bool discardFirstSample = true;
uint8_t presenceChangeCount = 0;
uint32_t latestIR = 0;

float temperatureC = 0;
bool temperatureValid = false;
bool temperaturePending = false;
bool temperatureStarted = false;
unsigned long temperatureStartTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastSampleTime = 0;

void resetMeasurements();
void processSample(uint32_t red, uint32_t ir);
void updateTemperature(unsigned long now);
void printResults(unsigned long now);

void resetMeasurements() {
  samplesStored = 0;
  heartRate = 0;
  spo2 = 0;
  validHeartRate = 0;
  validSPO2 = 0;
  calculationReady = false;
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(10);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("ERROR: MAX30102 not found. Check power/SDA/SCL."));
    while (true) delay(1000);
  }

  // Brightness 60, average 4, Red+IR, 100 samples/s, 411 us, 4096 nA.
  // FIFO output = 25 sample pairs/s, matching the Maxim algorithm.
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.enableDIETEMPRDY();
  lastSampleTime = millis();

  // The spaced INFO label keeps startup text out of the IDE 2 plotter.
  Serial.println(F("INFO: Place your finger steadily on the sensor."));
  Serial.println(F("INFO: BPM/SpO2 need about 4 seconds after detection."));
}

void updateTemperature(unsigned long now) {
  if (!temperaturePending) {
    if (temperatureStarted &&
        now - temperatureStartTime < TEMP_INTERVAL_MS) return;

    particleSensor.getINT2(); // Clear a previous temperature-ready flag.
    // MAX30102 register 0x21: start one temperature conversion.
    particleSensor.writeRegister8(MAX30105_ADDRESS, 0x21, 0x01);
    temperatureStartTime = now;
    temperatureStarted = true;
    temperaturePending = true;
    return;
  }

  if (particleSensor.getINT2() & 0x02) {
    int8_t whole = (int8_t)particleSensor.readRegister8(
        MAX30105_ADDRESS, 0x1F);
    uint8_t fraction = particleSensor.readRegister8(
        MAX30105_ADDRESS, 0x20);
    temperatureC = whole + (fraction & 0x0F) * 0.0625f;
    temperatureValid = true;
    temperaturePending = false;
  } else if (now - temperatureStartTime >= TEMP_TIMEOUT_MS) {
    temperatureValid = false;
    temperaturePending = false;
  }
}

void processSample(uint32_t red, uint32_t ir) {
  latestIR = ir;
  bool detected = fingerPresent ? ir >= FINGER_OFF_IR : ir >= FINGER_ON_IR;

  // Require three consecutive samples before changing presence state.
  if (detected != fingerPresent) {
    resetMeasurements();
    if (++presenceChangeCount >= PRESENCE_CONFIRM_SAMPLES) {
      fingerPresent = detected;
      presenceChangeCount = 0;
    }
    return;
  }
  presenceChangeCount = 0;

  if (!fingerPresent) {
    signalClipped = false;
    resetMeasurements();
    return;
  }

  // Reject samples at the 18-bit ADC ceiling.
  signalClipped = red >= 262140UL || ir >= 262140UL;
  if (signalClipped) {
    resetMeasurements();
    return;
  }

  redBuffer[samplesStored] = red;
  irBuffer[samplesStored] = ir;
  ++samplesStored;

  if (samplesStored == SAMPLE_COUNT) {
    maxim_heart_rate_and_oxygen_saturation(
        irBuffer, SAMPLE_COUNT, redBuffer,
        &spo2, &validSPO2, &heartRate, &validHeartRate);
    calculationReady = true;

    // Retain the newest 75 samples; the next 25 provide the next update.
    for (uint16_t i = UPDATE_COUNT; i < SAMPLE_COUNT; ++i) {
      redBuffer[i - UPDATE_COUNT] = redBuffer[i];
      irBuffer[i - UPDATE_COUNT] = irBuffer[i];
    }
    samplesStored = SAMPLE_COUNT - UPDATE_COUNT;
  }
}

void printResults(unsigned long now) {
  bool dataMissing = now - lastSampleTime >= DATA_TIMEOUT_MS;

  Serial.print(F("BPM: "));
  if (fingerPresent && calculationReady && validHeartRate == 1)
    Serial.print(heartRate);
  else
    Serial.print(F("--"));

  Serial.print(F(" | SpO2:"));
  if (fingerPresent && calculationReady && validSPO2 == 1) {
    Serial.print(spo2);
    Serial.print(F(" %"));
  } else {
    Serial.print(F("--"));
  }

  Serial.print(F(" | Chip temp: "));
  if (temperatureValid && !dataMissing) {
    Serial.print(temperatureC, 2);
    Serial.print(F(" C"));
  } else {
    Serial.print(F("--"));
  }

  Serial.print(F(" | IR: "));
  Serial.print(latestIR);

  if (dataMissing)
    Serial.print(F(" | WARNING: Sensor data missing"));
  else if (!fingerPresent)
    Serial.print(F(" | WARNING: No finger/presence detected"));
  else if (signalClipped)
    Serial.print(F(" | WARNING: Signal clipped; reduce LED brightness"));
  else if (!calculationReady) {
    Serial.print(F(" | Collecting samples: "));
    Serial.print(samplesStored);
    Serial.print(F("/100"));
  } else if (validHeartRate != 1 || validSPO2 != 1)
    Serial.print(F(" | Reading invalid; keep finger steady"));

  Serial.println();
}

void loop() {
  unsigned long now = millis();
  // Reset the measurement window if the sample stream is interrupted.
  if (now - lastSampleTime > 200) resetMeasurements();
  if (now - lastSampleTime >= DATA_TIMEOUT_MS) {
    fingerPresent = false;
    presenceChangeCount = 0;
    temperatureValid = false;
  }

  particleSensor.check();
  while (particleSensor.available()) {
    uint32_t red = particleSensor.getFIFORed();
    uint32_t ir = particleSensor.getFIFOIR();
    particleSensor.nextSample();
    lastSampleTime = millis();

    if (discardFirstSample) {
      discardFirstSample = false;
      continue;
    }
    processSample(red, ir);
  }

  now = millis();
  updateTemperature(now); // No waiting loop: pulse sampling keeps running.
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    printResults(now);
  }
  delay(1);
}
