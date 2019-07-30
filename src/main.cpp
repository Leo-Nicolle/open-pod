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
  MP3Display display = MP3Display(&state);
  MP3MusicShield musicShield = MP3MusicShield(&state);
  state.init();
  display.drawLines();

  delay(100);
  // state.incrementLine();
  display.drawLines();
  delay(1000);
  // state.incrementLine();
  // display.drawLines();
  state.forward();
  display.drawLines();
  delay(1000);

  state.forward();
  display.drawLines();
  delay(1000);

  state.forward();
  display.drawLines();
  delay(1000);
  // musicShield.playFile();
  Serial.println("end init");

}
void loop(){
  delay(100);
}
