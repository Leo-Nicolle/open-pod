
#include "display.h"

MP3Display::MP3Display(State* state){
  this->state = state;
  display.begin(UCG_FONT_MODE_SOLID);
  display.clearScreen();
  display.setPrintDir(3);
  display.setFontPosTop();
  display.setFont(ucg_font_helvB08_tf);
}

void MP3Display::drawLines(){
  display.setFont(ucg_font_helvB08_tf);
  display.setPrintPos(X_FIRST_LINE,Y_FIRST_LINE);
  display.print(state->getMenuState().title);

  for(int i = 0; i < NUM_DISPLAY_LINES; i++){
    display.setPrintPos(LINE_LINEHEIGHT * (i+1) + X_FIRST_LINE,Y_FIRST_LINE);
    char * line = state->lines[i];
    display.print(line);
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
  display.setPrintPos( X_FIRST_LINE,30);
  // display.println(battery);
  char barWidth = 4;
  char barHeight = 7;
  char numBars = 3;
  for(char i = 0; i< numBars; i++){
    display.drawBox(X_FIRST_LINE, 30- i* (barWidth+2), barHeight, barWidth);
  }
  if(battery>= 0.9){
    display.drawBox(
       X_FIRST_LINE+ barHeight/2-1,
       30- numBars* (barWidth+2)+barWidth/2,
       barHeight/2, barWidth/2);
  }
}

void MP3Display::update(){
  display.clearScreen();
  // display.fillScreen(ST77XX_BLACK);
  drawLines();
  drawBattery();
}
