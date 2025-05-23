
#include "player.h"

PodPlayer player;
void setup() {
  Serial.begin(9600);
  Serial.println("Adafruit VS1053 Simple Test");
  player.setup();
}

void loop() { 
 __WFI();

}