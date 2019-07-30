
#include "state.h"

State::State(){
  Serial.print("init state ");
  Serial.println(battery);
  // strcpy(this->line, "coucou");
}


void State::readDataLines(){
  Serial.print(data_file.position());
  char lineBuffer[128];
  data_file.seek(line_index_file*128);
  int i = 0;
  while(i < 6 && data_file.position() < data_file.size()){
      data_file.read(lineBuffer, 128);
      for(int j = 0; j< 128; j++){
        if(lineBuffer[j] == (char)1){
          lines[i][j]=0;
          break;
        }
        strcpy(lines[i][j],lineBuffer[j]);
      }

      while(i < 6 ){
        strcpy(lines[i][j],"");
        i++;
      }
}
// void State::readDataLine(int index){
//   int i = 0;
//   String buffer;
//   if (data_file.available()) {
//     buffer = data_file.readStringUntil('\n');
//      strcpy(lines[index] , buffer.c_str());
//      i++;
//   }
// }


void State::setDataFilePath(char * path){
  if(data_file){
    data_file.close();
  }
  strcpy(data_file_path, path);
  data_file = SD.open(data_file_path);
  line_index_file = 0;
  line_index=0;
  readDataLines();
}

void State::incrementLine(){
  line_index++;
}
void State::decrementLine(){
  line_index--;
}
