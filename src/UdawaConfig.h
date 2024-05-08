#ifndef UDAWACONFIG_H
#define UDAWACONFIG_H

#include <ArduinoJson.h>
#include "params.h"
#include "secret.h"

struct GenericConfig{
  char hwid[16];
  char name[24];
  char model[16];
  char group[16];
  uint8_t logLev;

  char tbAddr[48];
  uint16_t tbPort;
  char wssid[48];
  char wpass[48];
  char dssid[24];
  char dpass[24];
  char upass[64];
  char accTkn[24];
  bool provSent;

  char provDK[24];
  char provDS[24];

  int gmtOff;

  bool fIoT;
  bool fWOTA;
  bool fWeb;
  char hname[40];
  char htU[24];
  char htP[24];

  char logIP[16] = "255.255.255.255";
  uint16_t logPrt = 29514;
};

class UdawaGenericConfig{
    public:
        UdawaGenericConfig();
        void write(JsonDocument &doc, const char* configPath);
        void read(JsonDocument &doc, const char* configPath);
    private:
        GenericConfig _genericConfig;
};

#endif