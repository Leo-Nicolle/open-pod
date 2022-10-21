#include <Arduino.h>
#include <ota.h>

OTA ota = OTA();

void setup() {
  Serial.begin(115200);
  ota.setup();

}

void loop() {
  ota.update();
}
