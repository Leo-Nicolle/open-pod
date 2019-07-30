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
  // state.data_file="/mp3/";
  MP3Display display = MP3Display(&state);
  MP3MusicShield musicShield = MP3MusicShield(&state);

  state.setDataFilePath("/mp3/data/files/00000000.txt");
  strcpy(state.title , "artists");

  display.drawLines();
  // strcpy(state.audio_file_path,"/mp3/audio/00000000/00000000/00000000.mp3");
  // musicShield.playFile();
  // state.setDataFilePath("/mp3/data/files/00000000.txt");
  // display.drawLines();
  // delay(5000);
  // strcpy(state.audio_file_path,"/mp3/audio/00000000/00000000/00000002.mp3");
  // musicShield.playFile();
  // state.setDataFilePath("/mp3/data/files/00000000.txt");
}
void loop(){
  delay(100);
}
