
#include <debughttp.h>

void DebugHTTP::setup() {
  if(!(WiFi.waitForConnectResult() != WL_CONNECTED)){
    WiFi.begin(ssid, password);
  }
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
}

void DebugHTTP::post(String key, int value, boolean print) {
    // Your Domain name with URL path or IP address with path
  http.begin(client, serverURL + key);
  http.addHeader("Content-Type", "application/json");
  if(print){
    Serial.println(key + ": " + value);
  }
  int httpResponseCode = http.POST("{\"" + key + "\":" + String(value) + "}");
  if(httpResponseCode>0){
      String response = http.getString();  //Get the response to the request
      // Serial.println(httpResponseCode);   //Print return code
      // Serial.println(response);           //Print request answer
    
  }else{
      // Serial.print("Error on sending POST: ");
      // Serial.println(httpResponseCode); 
  }
}