
#include "display.h"

MP3Display::MP3Display(State* state){
  this->state = state;
  display.begin();
  display.setRotation(2);
  display.setContrast(100);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.clearDisplay();
}

void MP3Display::drawLines(){
  display.setCursor(0,0);
  char*line = state->lines;
  int i =0;
  while(i <6){
    display.println(line);
    Serial.println(line);
    line+=strlen(line);
    i++;
  }

  display.display();
}
//
// void displayTune(){
//   this->display.setCursor(0,0);
//   char*line = this->state->lines;
//   int i =0;
//   while(i <6){
//     this->display.println(line);
//     Serial.println(line);
//     line+=strlen(line);
//     i++;
//   }
//
//   this->display.display();
//
// }
