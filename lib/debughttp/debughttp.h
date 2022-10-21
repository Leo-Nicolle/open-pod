

#include <WiFi.h>
#include <HTTPClient.h>

#ifndef DEBUGHTTP_H
#define DEBUGHTTP_H
class DebugHTTP
{
  public:
   WiFiClient client;
   HTTPClient http;

  void setup();
  void post(int value);
};

#endif // DEBUGHTTP_H