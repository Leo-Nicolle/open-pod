
#include "display.h"

MP3Display::MP3Display(State* state){

  this->state = state;
  display.begin(UCG_FONT_MODE_SOLID);
  display.setPrintDir(3);
  display.clearScreen();
  display.setFontPosTop();
  display.setPrintPos(0,160);
  display.setFont(ucg_font_helvB08_tf);
  display.print("Does not work:");
}

void MP3Display::drawLines(){
  display.setColor(255, 255, 255);
  display.setFont(ucg_font_helvB08_tr);
  drawText(state->getMenuState().title, 0);

  for(int i = 0; i < 6; i++){
    char * line = state->lines[i];
    drawText(line, i+1);
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

void MP3Display::drawText(char* text, int line){
  if(strlen(texts[line].text)){
    // erase the previous text: 
    display.setPrintPos(0,160 - line * 8);
    display.setColor(0, 0, 0);
    display.print(texts[line].text);
  }
  //record the new line
  strcpy(texts[line].text, text);
  display.setPrintPos(0,160 - line * 8);
  display.setColor(255, 255, 255);
  display.print(texts[line].text);
 
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
