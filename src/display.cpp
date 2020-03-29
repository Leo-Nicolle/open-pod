
#include "display.h"

MP3Display::MP3Display(State* state){

  this->state = state;
  // tft.fillScreen(ST77XX_BLACK);
  // display.begin();
  // display.setRotation(2);
  // display.setContrast(50);
  // display.setTextColor(BLACK, WHITE);
  // display.setTextSize(1);
  // display.clearDisplay();
  // display.setCursor(0,0);
  // display.println("WAITING FOR SERIAL");
  // display.display();
  display.setCursor(0, 0);
  display.setTextColor(ST77XX_WHITE);
  display.setTextWrap(true);
  display.print("WAITING FOR SERIAL");
}

void MP3Display::drawLines(){
  display.setCursor(0,0);
  char title[128];
  display.println(state->getMenuState().title);
  for(int i = 0; i < 6; i++){
    char * line = state->lines[i];
    display.println(line);
  }
}


float MP3Display::checkBattery(){
 //This returns the current voltage of the battery on a Feather 32u4.
 float measuredvbat = analogRead(9);
 measuredvbat *= 2;    // we divided by 2, so multiply back
 measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
 measuredvbat /= 1024; // convert to voltage
 return measuredvbat;
}


void MP3Display::drawBattery(){
  float battery = checkBattery();
  display.setCursor(0,25);
  display.println(battery);
}

void MP3Display::update(){
  display.clearDisplay();
  drawLines();
  drawBattery();
  tft.fillScreen(ST77XX_BLACK);

}
