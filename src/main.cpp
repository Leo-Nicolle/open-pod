#include <Arduino.h>
#include <ota.h>
#include <debughttp.h>
#include <clickwheel.h>

OTA ota = OTA();
DebugHTTP debug;
ClickWheel clickWheel = ClickWheel();

unsigned long lastUpdateTime = 0;
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
  unsigned long currentTime = millis();

  int16_t deltaWheel = clickWheel.getWheelIncrement();  
  if(deltaWheel != 0){
    debug.post("angle", clickWheel.lastWheelAngle);
    debug.post("delta", deltaWheel, true);
  }else if (clickWheel.lastWheelAngle ==-1){
    debug.post("touched", -1);
    if(currentTime - lastUpdateTime >= 1000){
        clickWheel.takeWheelBaseline();
        lastUpdateTime= currentTime;
    }
  }else{
      debug.post("touched", 1);
  }
}
