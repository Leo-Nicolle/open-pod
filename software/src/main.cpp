// DRV2605L bring-up sketch. The full firmware is parked in main_save.cpp
// (excluded from the build in platformio.ini).
//
// Scans I2C3 (expect 0x5A DRV2605L + 0x5B MPR121), then plays every built-in
// effect (1..123) one after another, printing its number.
#include "Adafruit_DRV2605.h"
#include "pinout.h"
#include <Arduino.h>
#include <Wire.h>

// ERM (coin/eccentric motor) by default. Set to true for an LRA.
const bool USE_LRA = false;

TwoWire Wire3(I2C3_SDA, I2C3_SCL);
Adafruit_DRV2605 drv;

uint8_t effect = 1;

void scanI2C()
{
  Serial.println("I2C3 scan:");
  for (uint8_t addr = 1; addr < 127; addr++)
  {
    Wire3.beginTransmission(addr);
    if (Wire3.endTransmission() == 0)
    {
      Serial.print("  found 0x");
      Serial.println(addr, HEX);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=== DRV2605L test ===");

  Wire3.begin();
  Wire3.setClock(100000);
  scanI2C();

  // The library always talks to 0x5A (DRV2605_ADDR).
  if (!drv.begin(&Wire3))
  {
    Serial.println("❌ DRV2605L not found!");
    while (1)
      delay(1000);
  }
  Serial.println("✅ DRV2605L initialized!");

  if (USE_LRA)
  {
    drv.useLRA();
    drv.selectLibrary(6);
  }
  else
  {
    drv.useERM();
    drv.selectLibrary(1);
  }
  // Effects are fired by drv.go() over I2C; the IN pin is unused.
  drv.setMode(DRV2605_MODE_INTTRIG);
}

void loop()
{
  Serial.print("Effect #");
  Serial.println(effect);

  drv.setWaveform(0, effect); // slot 0: the effect
  drv.setWaveform(1, 0);      // slot 1: end of sequence
  drv.go();

  delay(1000);

  effect++;
  if (effect > 123)
    effect = 1;
}
