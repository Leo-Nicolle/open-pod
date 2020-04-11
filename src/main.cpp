#include  "state.h"
#include  "display.h"
#include  "music-shield.h"
#include  "touch.h"


#include <CapacitiveSensor.h>

#include <Arduino.h>


State* state;
MP3Display* display;
MP3MusicShield* musicShield;
Touch* touch;

char switchIndex = 0;

void setup(){
 
  state = new State();

  display = new MP3Display(state);
  Serial.begin(9600);
  // display->drawLines();

  musicShield = new MP3MusicShield(state);
  state->init();
  state->forward();
  state->forward();
  state->incrementLine();
  state->incrementLine();
  state->incrementLine();

  // state->backward();
  display->update();



  // Serial.println("IniT");
  // display->update();

  // Serial.println("IniT3");

  // touch = new Touch();
  // state->forward();
  // state->forward();
  // state->forward();


  // delay(100);
  // state->incrementLine();
  // delay(1000);
  // state->incrementLine();
  // display->drawLines();
  // display->drawLines();
  // delay(500);

  // state->forward();
  // display->drawLines();
  // state->incrementLine();
  // delay(500);

  // state->forward();
  // display->drawLines();
  // delay(500);

  // state->backward();
  // display->drawLines();
  // delay(500);
  // strcpy(state->audio_file_path,"/mp3/audio/00000000/00000000/00000000.mp3");
  // musicShield->playFile();
  // Serial.println("end init");

}
void loop(){
  delay(1000);
  // if(switchIndex==0){
  // state->forward();
  state->decrementLine();
  // }else{
  // state->backward();
  // }
  switchIndex=(switchIndex + 1)%2;

  display->update();
}
