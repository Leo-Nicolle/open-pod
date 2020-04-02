
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
        strcpy(root_music_path, "/mp3/audio/");

}

void State::init(){
  setDataFilePath("/mp3/data/files/00000000.txt");
}


void State::readDataLines(){
    char lineBuffer[128];
    int i = 0;
    data_file.seek(line_index_file*128);
    while(i < min(NUM_LINES, max_lines)) {
      if(data_file.position() >= data_file.size()){
          data_file.seek(0);
      }
      data_file.read(lineBuffer, 128);
      int index = strchr(lineBuffer, (char)1) - lineBuffer;
      Serial.println("index ici ");
      Serial.println(index);

      strncpy(lines[i], lineBuffer, index);
      for(int j=index; j< 128 ; j++){
        lines[i][j]=0;
      }
      lines[i][index]=0;
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
  max_lines = data_file.size()/128;
  Serial.print("max lines ");
  Serial.print(data_file.size());
  Serial.print(" ");
  Serial.println(max_lines);
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
  data_file.read(buffer, 128);
  char* endFirstPart=  strchr(buffer, (char)1);
  char* endSecondPart= strchr(endFirstPart+1, (char)1);
  int firstSeparatorIndex = (int) (endFirstPart - buffer );
  int secondSeparatorIndex = (int) (endSecondPart - endFirstPart-1 );


  Serial.println(buffer);
  char description[128];
  if(firstSeparatorIndex > 0 && firstSeparatorIndex < 128){
    strncpy(description, buffer,firstSeparatorIndex);
    description[firstSeparatorIndex] = 0;
    Serial.println(description);
  }
  char path[128];
  if(secondSeparatorIndex > 0 && secondSeparatorIndex < 128 - firstSeparatorIndex){
    strncpy(path, endFirstPart+1, secondSeparatorIndex );
    path[secondSeparatorIndex] = 0;
    Serial.println(path);
  }

  char* txt = strstr(path, ".txt");
  if(txt == NULL){
    Serial.println("audio file");
    Serial.println(path);

    // audio file, play it
    strcpy(audio_file_path, root_music_path);
    strcat(audio_file_path, path);
    return 1;
  }else{
    //data file, display it
    Serial.print("data file " );
    Serial.println(path);
    menuStates[menuIndex].line = line_index_file;
    line_index_file = 0;
    // menuIndex++;

    strcpy(menuStates[menuIndex].path, root_data_path);
    strcat(menuStates[menuIndex].path, path);
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
