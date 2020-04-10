
#include "display.h"

MP3Display::MP3Display(State* state){

  // display.setPrintPos(0,160);
  // display.setFont(ucg_font_helvB08_tf);
  // display.print("Does not work:");

  this->state = state;
  display.begin(UCG_FONT_MODE_SOLID);
  // display.setPrintDir(3);
  display.clearScreen();
  display.setColor(255, 255, 255);
  display.drawRBox(0,0,128,80,1);
  display.setColor(255, 0, 0);
  display.drawRBox(0,80,128,80,1);

  // display.setFontPosTop();
  // display.setPrintPos(0,160);
  // display.print("coucou");
  // display.setPrintPos(40,40);
  // display.print("coucou");
  // display.setPrintPos(100,0);
  // display.print("coucou");
  // display.setPrintPos(0,0);
  // display.print("coucou");

  display.setFont(ucg_font_helvB08_tf);
 
}

void MP3Display::drawLines(){
  display.setFont(ucg_font_helvB08_tf);

  // drawText(state->getMenuState().title, 0);
  for(int i = 0; i < NUM_DISPLAY_LINES; i++){
    display.setPrintPos(0,Y_FIRST_LINE - LINE_LINEHEIGHT * i);
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
  // float battery = checkBattery();
  // display.setCursor(0,25);
  // display.println(battery);
}

void MP3Display::update(){
  // display.fillScreen(ST77XX_BLACK);
  // drawLines();
  // drawBattery();
}
