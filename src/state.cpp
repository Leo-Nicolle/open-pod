
#include "state.h"

State::State(){
}

void State::init(){
  Serial.print("init state ");
  strcpy(menuStates[0].title, "Menu");
  strcpy(menuStates[1].title, "Artists");
  strcpy(menuStates[2].title, "Albums");
  strcpy(menuStates[3].title, "Tracks");
  for (int i = 0; i < 4; i++)
  {
    menuStates[i].line = 0;
    strcpy(menuStates[i].path, "");
  }
  menuIndex = 1;
  strcpy(root_data_path, "/mp3/data/files/");
  strcpy(root_music_path, "/mp3/audio/");

  setDataFilePath((char *)"/mp3/data/files/00000000.txt");
}


void State::readDataLines(){
    char lineBuffer[128];
    int i = 0;
    data_file.seek(getMenuStatePointer()->line*128);
    while(i <  max_lines) {
      if(data_file.position() >= data_file.size()){
          data_file.seek(0);
      }
      data_file.read((uint8_t *)lineBuffer, 128);
      int index = strchr(lineBuffer, (char)1) - lineBuffer;

      strncpy(lines[i], lineBuffer, index);
      lines[i][index]=0;
      i++;
    }
}

void State::cleanLines(){
  for(short i=0;i< NUM_LINES; i++){
    lines[i][0]= 0;
  }
}

void State::setDataFilePath(char * path){
  Serial.println("set menu");
  Serial.println(path);

  if(data_file) {
    data_file.close();
  }
  strcpy(menuStates[menuIndex].path, path);
  data_file = SD.open(menuStates[menuIndex].path);
  max_lines = data_file.size()/128;
  cleanLines();
  readDataLines();
}

void State::incrementLine(){
    menuState* currentMenu = getMenuStatePointer(); 
    if(currentMenu->selectedLine<5){
      currentMenu->selectedLine = currentMenu->selectedLine + 1;
    }else{
      currentMenu->line = (currentMenu->line + 1) % max_lines;
      readDataLines();
    }
}
void State::decrementLine(){
    menuState* currentMenu = getMenuStatePointer(); 
    if(currentMenu->selectedLine>0){
      currentMenu->selectedLine = currentMenu->selectedLine - 1;
    }else{
      currentMenu->line -=1;
      if(currentMenu->line < 0){
        currentMenu->line = max_lines - currentMenu->line-1;
      }
      readDataLines();
    }
}


int State::forward(){
  menuState* currentMenu = getMenuStatePointer();
  data_file.seek((currentMenu->line + currentMenu->selectedLine)*128);
  char buffer[128];
  data_file.read((uint8_t *)buffer, 128);
  char* endFirstPart=  strchr(buffer, (char)1);
  char* endSecondPart= strchr(endFirstPart+1, (char)1);
  int firstSeparatorIndex = (int) (endFirstPart - buffer );
  int secondSeparatorIndex = (int) (endSecondPart - endFirstPart-1 );


  char description[128];
  if(firstSeparatorIndex > 0 && firstSeparatorIndex < 128){
    strncpy(description, buffer,firstSeparatorIndex);
    description[firstSeparatorIndex] = 0;
  }
  char path[128];
  if(secondSeparatorIndex > 0 && secondSeparatorIndex < 128 - firstSeparatorIndex){
    strncpy(path, endFirstPart+1, secondSeparatorIndex );
    path[secondSeparatorIndex] = 0;
  }

  char* txt = strstr(path, ".txt");
  if(txt == NULL){
    // audio file, play it
    strcpy(audio_file_path, root_music_path);
    strcat(audio_file_path, path);
    return 1;
  }else{
  
    //data file, display it

    menuIndex++;
    menuState* currentMenu = getMenuStatePointer();
    currentMenu->line = 0;
    strcpy(currentMenu->path, root_data_path);
    strcat(currentMenu->path, path);
    setDataFilePath(currentMenu->path);
    return 0;
  }
}

void State::backward(){
  menuIndex-=1;
  setDataFilePath(getMenuState().path);
}

menuState State::getMenuState(){
  return menuStates[menuIndex];
}
menuState* State::getMenuStatePointer(){
  return menuStates+menuIndex;
}