#include  "state.h"
#include  "display.h"
#include  "music-shield.h"

#include <Arduino.h>


void setup(){
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }
  Serial.println("IniT");
  State state = State();
  MP3Display display = MP3Display(state);
  MP3MusicShield musicShield = MP3MusicShield(&state);

  // delay(100);
  //
  // memcpy(state.lines,"\0line1\0line2\0line3\0line4", 24);
  //
  //
  // // Serial.println(state.line);
  // display.drawLines();
  // delay(5000);


}
void loop(){
  delay(100);
}
