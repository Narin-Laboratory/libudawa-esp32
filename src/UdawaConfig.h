#ifndef UDAWACONFIG_H
#define UDAWACONFIG_H

#include <ArduinoJson.h>
#include <UdawaLogger.h>
#include <FS.h>
#include <LittleFS.h>
#include "secret.h"
#include "params.h"
#include <functional> 
#include <vector>

struct UdawaConfigStruct{
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
  #ifdef USE_IOT
  bool fIoT;
  char accTkn[24];
  bool provSent;
  char provDK[24];
  char provDS[24];
  #endif

  int gmtOff;

  bool fWOTA;
  bool fWeb;
  char hname[40];
  char htU[24];
  char htP[24];

  char logIP[16] = "255.255.255.255";
  uint16_t logPort = 29514;
};

extern SemaphoreHandle_t xSemaphoreConfig; 

class UdawaConfig{
    public:
        UdawaConfig(const char* path);
        bool begin();
        bool load();        
        bool save();
        UdawaConfigStruct state;
    private:
        UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        const char *_path;
        
};

class GenericConfig{
  public:
    GenericConfig(const char* path);
    bool load(JsonDocument &data);
    bool save(JsonDocument &data);
  private:
    UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
    const char *_path;
};

#endif