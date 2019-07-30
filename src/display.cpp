
#include "display.h"

MP3Display::MP3Display(State* state){
  this->state = state;
  display.begin();
  display.setRotation(2);
  display.setContrast(50);
  display.setTextColor(BLACK, WHITE);
  display.setTextSize(1);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WAITING FOR SERIAL");
  display.display();
}

void MP3Display::drawLines(){
  display.setRotation(2);
  display.setContrast(50);
  display.setTextColor(BLACK, WHITE);
  display.setTextSize(1);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("NICE!");

  // int i = 0;
  // char str[256];
  // while(i <6){
  //   char * line = state->lines[state->line_in_lines+i];
  //   for(int j =0; j< 256; j++){
  //     if(line[j] < (char)1){
  //       str[j]=(char)0;
  //       break;
  //     }
  //     str[j]=line[j];
  //   }
  //   display.println(str);
  //   Serial.println(str);
  //   i++;
  // }
  display.display();
  delay(2000);
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
