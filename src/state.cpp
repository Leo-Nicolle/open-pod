
#include "state.h"

State::State(){
        Serial.print("init state ");
        Serial.println(battery);
        strcpy(menuStates[0].title, "Menu");
        strcpy(menuStates[1].title, "Artists");
        strcpy(menuStates[2].title, "Albums");
        strcpy(menuStates[3].title, "Tracks");
        for(int i=0;i<4;i++){
          menuStates[i].line = 0;
          strcpy(menuStates[i].path,"");
        }
        menuIndex=1;
        strcpy(root_data_path, "/mp3/data/files/");
}

void State::init(){
  setDataFilePath("/mp3/data/files/00000000.txt");
}


void State::readDataLines(){
    char lineBuffer[128];
    int i = 0;
    data_file.seek(line_index_file*128);
    while(i < min(6, max_lines)) {
      // Serial.println("ICICI");
      // Serial.print(data_file.size());
      // Serial.print(" -- ");
      // Serial.println(data_file.position());

      if(data_file.position() >= data_file.size()){
          data_file.seek(0);
      }
      data_file.read(lineBuffer, 128);
      for(int j = 0; j< 128; j++) {
          if(lineBuffer[j] == (char)1) {
              lines[i][j]=0;
              break;
          }
          lines[i][j]=lineBuffer[j];
      }
      i++;
    }
    while(i < 6) {
      lines[i][0]=(char)0;
      i++;
    }
}

void State::setDataFilePath(char * path){
  Serial.println("set menu");
  Serial.println(path);

  if(data_file) {
    Serial.println("CLOSE");
    data_file.close();
  }
  strcpy(menuStates[menuIndex].path, path);
  data_file = SD.open(menuStates[menuIndex].path);
  line_index_file = 0;
  max_lines = data_file.size()/128;
  // Serial.print("max lines ");
  // Serial.print(data_file.size());
  // Serial.print(" ");
  // Serial.println(max_lines);
  readDataLines();
}

void State::incrementLine(){
    line_index_file = (line_index_file + 1) % max_lines;
    readDataLines();
}
void State::decrementLine(){
    line_index_file = (line_index_file + max_lines - 1) % max_lines;
    readDataLines();
}
int State::forward(){
  data_file.seek(line_index_file*128);
  char buffer[128];
  char path[128];

  data_file.read(buffer, 128);

  int found = 0;
  for(int i=0; i< 128; i++){
    if(found){
      path[i-found] = buffer[i];
    }
    if(buffer[i] == (char)1){
      if(found){
        path[i-found]=0;
        break;
      }
      found = i+1;
    }
  }
  char* txt = strstr(path, ".txt");
  if(txt == NULL){
    Serial.println("audio file");
    // audio file, play it
    return 1;
  }else{
    //data file, display it
    Serial.print("data file " );
    Serial.println(path);
    menuStates[menuIndex].line = line_index_file;
    menuIndex++;
    char completePath[128];
    strcpy(completePath, root_data_path);
    strcat(completePath, path);
    strcpy(menuStates[menuIndex].path, completePath);
    setDataFilePath(menuStates[menuIndex].path);
    return 0;
  }
}

void State::backward(){
  menuIndex--;
  line_index_file = getMenuState().line;
  setDataFilePath(getMenuState().path);
}

menuState State::getMenuState(){
  return menuStates[menuIndex];
}
