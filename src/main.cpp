#include <Arduino.h>
#include <ota.h>
#include <debughttp.h>
#include <clickwheel.h>

OTA ota = OTA();
DebugHTTP debug;
ClickWheel clickWheel = ClickWheel();

int angle = 0;


void setup() {
  Serial.begin(115200);
  ota.setup();

  delay(200);
  if (!clickWheel.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");
  clickWheel.initTouchWheel(4, 90 ,1);
  delay(100);
  clickWheel.takeWheelBaseline();
}

void loop() {
  ota.update();

  int16_t deltaWheel = clickWheel.getWheelIncrement();  
  if(deltaWheel != 0){
    angle += deltaWheel;
    debug.post("angle", clickWheel.lastWheelAngle);
  }else if (clickWheel.lastWheelAngle ==-1){
    // not touched
    // debug.post(clickWheel.lastWheelAngle);

  }
}
