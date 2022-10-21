
#include <debughttp.h>

void DebugHTTP::setup() {
  // const char* ssid = "4G-Gateway-1B52";
  // const char* password = "9NG4AT1NARF";


  // if(!(WiFi.waitForConnectResult() != WL_CONNECTED)){
  //   WiFi.begin(ssid, password);
  // }
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.println("Connection Failed! Rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }
}

void DebugHTTP::post(int value) {
    // Your Domain name with URL path or IP address with path
  http.begin(client, "http://192.168.8.137:3011/angle");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST("{\"angle\":" + String(value) + "}");
  if(httpResponseCode>0){
      String response = http.getString();  //Get the response to the request
      Serial.println(httpResponseCode);   //Print return code
      Serial.println(response);           //Print request answer
    
  }else{
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode); 
  }
}