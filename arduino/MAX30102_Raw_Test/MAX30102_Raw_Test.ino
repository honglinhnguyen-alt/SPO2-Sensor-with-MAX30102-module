/*
  MAX30102 raw-signal test for Arduino Uno R4 WiFi.
  Requires SparkFun MAX3010x Pulse and Proximity Sensor Library.
  Use default Wire SDA/SCL pins; Serial Monitor/Plotter: 115200 baud.
  Matches the previous sketch's LED current, ADC range and sample rate.
  Output contains unfiltered 18-bit Red and IR ADC counts.
  Set PLOT_IR_ONLY=true to inspect IR pulses without a different Red baseline
  compressing the Plotter's vertical scale. Use false to inspect both channels.
*/

#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor;

const bool PLOT_IR_ONLY = false;
const byte LED_BRIGHTNESS = 60;
const int ADC_RANGE = 4096;
bool discardFirstSample = true;
unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30102 not found. Check power, SDA and SCL."));
    while (true) {
      delay(1000);
    }
  }

  // Red + IR, 100 conversions/s with averaging of 4 -> 25 pairs/s.
  particleSensor.setup(LED_BRIGHTNESS, 4, 2, 100, 411, ADC_RANGE);
  lastSampleTime = millis();
}

void loop() {
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

    if (!PLOT_IR_ONLY) {
      Serial.print(F("Red:"));
      Serial.print(red);
      Serial.print('\t');
    }
    Serial.print(F("IR:"));
    Serial.println(ir);
  }

  if (millis() - lastSampleTime >= 1000) {
    Serial.println(F("No samples received. Check connections."));
    lastSampleTime = millis();
  }
  delay(1);
}
