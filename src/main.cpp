#include  "player.h"
#include  "display.h"
#include <Arduino.h>


void setup(){
  Serial.begin(9600);
  delay(5000);


  Serial.println("IniT");
  Player player = Player();
  MP3Display display = MP3Display(player);

}
void loop(){
  delay(10);
}
