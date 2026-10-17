/*
  MAX30102 heart-rate and SpO2 demo.
  Requires SparkFun MAX3010x Pulse and Proximity Sensor Library.
  Uses the board's default Wire SDA/SCL pins. Serial Monitor: 115200 baud.
  Algorithm supplied by Maxim through the SparkFun library:
  https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
*/

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor; // This class also supports MAX30102.

const uint8_t SAMPLE_COUNT = 100;
const uint8_t UPDATE_COUNT = 25;
const unsigned long SAMPLE_TIMEOUT_MS = 1000;

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
uint16_t irBuffer[SAMPLE_COUNT];
uint16_t redBuffer[SAMPLE_COUNT];
#else
uint32_t irBuffer[SAMPLE_COUNT];
uint32_t redBuffer[SAMPLE_COUNT];
#endif

int32_t spo2 = 0;
int8_t validSPO2 = 0;
int32_t heartRate = 0;
int8_t validHeartRate = 0;

bool windowReady = false;
bool discardFirstSample = true;

void configureSensor();
bool readSamples(uint8_t first, uint8_t end);
void calculateAndPrint();

void setup() {
  Serial.begin(115200);
  // Allow native-USB boards a short connection window; no key is required.
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }

  Serial.println(F("Initializing MAX30102..."));

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30102 not found. Check power, SDA and SCL."));
    while (true) {
      delay(1000);
    }
  }

  configureSensor();
  Serial.println(F("Place your finger steadily on the sensor."));
  Serial.println(F("First result takes about 4 seconds."));
}

void configureSensor() {
  const byte ledBrightness = 60;
  const byte sampleAverage = 4;
  const byte ledMode = 2; // Red + IR.
  const int sampleRate = 100;
  const int pulseWidth = 411;
  const int adcRange = 4096;

  // 100 samples/s averaged in groups of 4 -> 25 FIFO samples/s.
  particleSensor.setup(ledBrightness, sampleAverage, ledMode,
                       sampleRate, pulseWidth, adcRange);
  windowReady = false;
  discardFirstSample = true;
}

bool readSamples(uint8_t first, uint8_t end) {
  uint8_t i = first;
  while (i < end) {
    unsigned long start = millis();
    while (!particleSensor.available()) {
      particleSensor.check();
      if (millis() - start >= SAMPLE_TIMEOUT_MS) {
        return false;
      }
      delay(1);
    }

    uint32_t red = particleSensor.getFIFORed();
    uint32_t ir = particleSensor.getFIFOIR();
    particleSensor.nextSample();

    // Discard one startup sample before building the measurement window.
    if (discardFirstSample) {
      discardFirstSample = false;
      continue;
    }

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    // Scale both 18-bit channels to 16 bits, avoiding wraparound on AVR.
    redBuffer[i] = (uint16_t)(red >> 2);
    irBuffer[i] = (uint16_t)(ir >> 2);
#else
    redBuffer[i] = red;
    irBuffer[i] = ir;
#endif
    ++i;
  }
  return true;
}

void calculateAndPrint() {
  maxim_heart_rate_and_oxygen_saturation(
      irBuffer, SAMPLE_COUNT, redBuffer,
      &spo2, &validSPO2, &heartRate, &validHeartRate);

  Serial.print(F("HR: "));
  if (validHeartRate == 1) {
    Serial.print(heartRate);
    Serial.print(F(" bpm"));
  } else {
    Serial.print(F("Invalid"));
  }

  Serial.print(F(" | SpO2: "));
  if (validSPO2 == 1) {
    Serial.print(spo2);
    Serial.print(F(" %"));
  } else {
    Serial.print(F("Invalid"));
  }
  Serial.println();
}

void loop() {
  uint8_t first = 0;

  if (windowReady) {
    // Retain 75 samples, then collect 25 new samples.
    for (uint8_t i = UPDATE_COUNT; i < SAMPLE_COUNT; ++i) {
      redBuffer[i - UPDATE_COUNT] = redBuffer[i];
      irBuffer[i - UPDATE_COUNT] = irBuffer[i];
    }
    first = SAMPLE_COUNT - UPDATE_COUNT;
  }

  if (!readSamples(first, SAMPLE_COUNT)) {
    Serial.println(F("Sample timeout. Check connections; restarting."));
    validHeartRate = 0;
    validSPO2 = 0;
    configureSensor();
    return;
  }

  windowReady = true;
  calculateAndPrint();
}
