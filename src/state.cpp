
#include "state.h"

State::State(){
        Serial.print("init state ");
        Serial.println(battery);
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

  if(data_file) {
    Serial.println("CLOSE");
          data_file.close();
  }
  strcpy(data_file_path, path);
  data_file = SD.open(data_file_path);
  line_index_file = 0;
  line_index_file=0;
  max_lines = data_file.size()/128;
  Serial.print("max lines ");
  Serial.print(data_file.size());
  Serial.print(" ");
  Serial.println(max_lines);

  readDataLines();
}

void State::incrementLine(){
        line_index_file = (line_index_file + 1) % max_lines;
        Serial.print("line ");
        Serial.println(line_index_file);

        readDataLines();
}
void State::decrementLine(){
        line_index_file = (line_index_file + max_lines - 1) % max_lines;
        readDataLines();
}
