

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <constants.h>

#ifndef OTA_H
#define OTA_H
class OTA
{
  public:
  void setup();
  void update();
};

#endif // OTA_H