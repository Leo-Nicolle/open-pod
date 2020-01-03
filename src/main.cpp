#include  "state.h"
// #include  "display.h"
#include  "music-shield.h"

#include <CapacitiveSensor.h>

#include <Arduino.h>


State* state;
// MP3Display* display;
MP3MusicShield* musicShield;


void setup(){
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }
  Serial.println("IniT");
  state = new State();
  state->init();
  //
  // Serial.println("IniT1");
  //
  // display = new MP3Display(state);
  // display->drawLines();
  // display->update();

  Serial.println("IniT2");
  
  musicShield = new MP3MusicShield(state);
  Serial.println("IniT3");

  // touch = new Touch();
  // state->forward();
  // state->forward();
  // state->forward();
  //

  strcpy(state->audio_file_path,"/mp3/audio/00000000/00000000/00000000.mp3");

/*
  delay(100);
  // state.incrementLine();
  delay(1000);
  // state.incrementLine();
  // display.drawLines();
  display->drawLines();
  delay(500);

  state->forward();
  display->drawLines();
  state->incrementLine();
  delay(500);

  state->forward();
  display->drawLines();
  delay(500);

  state->backward();
  display->drawLines();
  delay(500*/
  musicShield->playFile();
  Serial.println("end init");

}
void loop(){
  delay(100);
  // Serial.print("check battery ");
  // Serial.println(checkBattery());
  // display->update();


}
