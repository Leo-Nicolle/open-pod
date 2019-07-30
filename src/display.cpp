
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
  display.println(state->getMenuState().title);

  for(int i = 0; i < 6; i++){
    char * line = state->lines[i];
    display.println(line);
  }
  display.display();
  delay(2000);
}
