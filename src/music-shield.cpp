
#include "music-shield.h"

MP3MusicShield::MP3MusicShield(State* state){
  this->state = state;
  Serial.print("state value");
  Serial.println(this->state->battery);


  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println("VS1053 initialized");

  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT)){
   Serial.println(F("DREQ pin is not an interrupt pin"));
}
  delay(400);

  Serial.print("Initializing SD card...");
   if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }



  musicPlayer.setVolume(20,20);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
}

void MP3MusicShield::playFile(){
  if(musicPlayer.playingMusic){
    musicPlayer.stopPlaying();
  }
    musicPlayer.startPlayingFile(state->audio_file);

}
