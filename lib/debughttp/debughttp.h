

#include <WiFi.h>
#include <HTTPClient.h>

#include <constants.h>

#ifndef DEBUGHTTP_H
#define DEBUGHTTP_H
class DebugHTTP
{
  public:
   WiFiClient client;
   HTTPClient http;

  void setup();
  void post(String key, int value, boolean print = false);
};

#endif // DEBUGHTTP_H