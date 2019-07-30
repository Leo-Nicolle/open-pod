
#include "state.h"

State::State(){
  Serial.print("init state ");
  Serial.println(battery);
  // strcpy(this->line, "coucou");
}


void State::readDataLines(){
  int i = line_in_file;
  String buffer;
  while (data_file.available() && i < NUM_LINES ) {
    buffer = data_file.readStringUntil('\n');
     strcpy(lines[i] , buffer.c_str());
     Serial.println("read line");
     Serial.println(buffer);
     Serial.println(lines[i]);
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
  line_in_file = 0;
  line_in_lines=0;
  readDataLines();
}

void State::incrementLine(){
  line_in_file++;
}
void State::decrementLine(){
  line_in_file--;
}
