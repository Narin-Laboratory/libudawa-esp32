/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Function helper library for ESP32 based UDAWA multi-device firmware development
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/

#ifndef libudawa_h
#define libudawa_h

#include <secret.h>
#include <esp32-hal-log.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Arduino.h>
#include <thingsboard.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <ArduinoOTA.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define DOCSIZE 1500
#define COMPILED __DATE__ " " __TIME__
#define LOG_REC_SIZE 30
#define LOG_REC_LENGTH 192
#define PIN_RXD2 16
#define PIN_TXD2 17

const char* configFile = "/cfg.json";
const char* configFileCoMCU = "/comcu.json";
char logBuff[LOG_REC_LENGTH];
char _logRec[LOG_REC_SIZE][LOG_REC_LENGTH];
uint8_t _logRecIndex;
bool FLAG_IOT_RPC_SUBSCRIBE = false;

struct Config
{
  char hwid[16];
  char name[32];
  char model[16];
  char group[16];
  uint8_t logLev;

  char broker[128];
  uint16_t port;
  char wssid[64];
  char wpass[64];
  char upass[64];
  char accessToken[64];
  bool provSent;

  char provisionDeviceKey[24];
  char provisionDeviceSecret[24];

  uint8_t relayChannels[4];
};

struct ConfigCoMCU
{
  bool fPanic;
  float pEcKcoe;
  float pEcTcoe;
  float pEcVin;
  float pEcPpm;
  uint16_t pEcR1;
  uint16_t pEcRa;

  uint16_t bfreq;
  bool fBuzz;

  uint8_t pinBuzzer;
  uint8_t pinLedR;
  uint8_t pinLedG;
  uint8_t pinLedB;
  uint8_t pinEcPower;
  uint8_t pinEcGnd;
  uint8_t pinEcData;
};


void reboot();
char* getDeviceId();
void cbWifiOnConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void cbWiFiOnLostIp(WiFiEvent_t event, WiFiEventInfo_t info);
void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info);
void configLoadFailSafe();
void configLoad();
void configSave();
void configReset();
void configCoMCULoadFailSafe();
void configCoMCULoad();
void configCoMCUSave();
void configCoMCUReset();
bool loadFile(const char* filePath, char* buffer);
void processProvisionResponse(const Provision_Data &data);
void recordLog(uint8_t level, const char* fileName, int, const char* functionName);
void iotInit();
void startup();
void udawa();
void otaUpdateInit();
void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE> &doc, bool isRpc);
void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE> &doc);
void syncConfigCoMCU();
void readSettings(StaticJsonDocument<DOCSIZE> &doc,const char* path);
void writeSettings(StaticJsonDocument<DOCSIZE> &doc, const char* path);

static const char NARIN_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIGSjCCBDKgAwIBAgIJAMxU3KljbiooMA0GCSqGSIb3DQEBCwUAMIGwMQswCQYD
VQQGEwJJRDENMAsGA1UECAwEQmFsaTEQMA4GA1UEBwwHR2lhbnlhcjEhMB8GA1UE
CgwYQ1YuIE5hcmF5YW5hIEluc3RydW1lbnRzMSQwIgYDVQQLDBtOYXJpbiBDZXJ0
aWZpY2F0ZSBBdXRob3JpdHkxFjAUBgNVBAMMDU5hcmluIFJvb3QgQ0ExHzAdBgkq
hkiG9w0BCQEWEGNlcnRAbmFyaW4uY28uaWQwIBcNMjAwMTE2MDUyMjM1WhgPMjA1
MDAxMDgwNTIyMzVaMIGwMQswCQYDVQQGEwJJRDENMAsGA1UECAwEQmFsaTEQMA4G
A1UEBwwHR2lhbnlhcjEhMB8GA1UECgwYQ1YuIE5hcmF5YW5hIEluc3RydW1lbnRz
MSQwIgYDVQQLDBtOYXJpbiBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkxFjAUBgNVBAMM
DU5hcmluIFJvb3QgQ0ExHzAdBgkqhkiG9w0BCQEWEGNlcnRAbmFyaW4uY28uaWQw
ggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC7SU2ahwCe1KktoaUEQLmr
E91S2UwaqGvcGksy9j08GnI1NU1MpqsVrPSxuLQRr7ww2IG9hzKN0rKIhkXUfBCJ
X8/K7bxEkLl2yfcJjql90/EdAjWClo3CURM1zIqzxggkZKmdEGrPV/WGkYchmxuT
QvYDoPVLScXhN7NtTfzd3x/zWwe4WHg4THpfqeyE6vCLoNeDKvF2GsP0xsYtows8
pTjKH9gh0kFi+aYoVxjbH8KB78ktWAOo1T2db3KUF4/gYweYk4b/vS1egdDm821/
6qC7XrsnaApyRm73RKtmhAzldx9D1YqdVbFIx5oRlEg3+uI7hv/YD6Icfhazw1ql
Su8U7g8Ax8OPVdjdJ41lgkFs+OpY4GnfzjIzhvQ+kRIPyDQQaLwCxFZJZIa2jAnB
R5GzSM+Py0d+oStELotd0O3kLC7z4eFdxfRuaaQzofn/aUT1K7NsbG8V7rC3lG9P
8Jc+SU7zP/XSqVjKFTzRnIQ6C4WwkdWwS7uN2FSQBmlIw4EHaYcoMbQCVN2DcGFE
iMH+8/kp99UBKCu2MKq1zM+W0n+dNJ65EdeygOw580EIdNY5DPbpQTeNMt1idXF2
8C9jMyInGt6ZgZ5IfjExfDDIb6WYl3KRmZxqXjWCMg1e5TwMrbeoeg3R6kFY3lG4
TReG7Zjp1PQ3vxMC4ZWcpQIDAQABo2MwYTAdBgNVHQ4EFgQU4rkKARnad9D4bdWs
t3Jo8KKTxHAwHwYDVR0jBBgwFoAU4rkKARnad9D4bdWst3Jo8KKTxHAwDwYDVR0T
AQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwDQYJKoZIhvcNAQELBQADggIBAJAY
pl1fdOG8GpvTIDts/H1CZbT6bF85+FnAV6abL/X4dIx/sQpg9TASAicyMzqUtDSb
2WaD3tkDvFhu7/vzG62x0tcpGw99Bxy0pkSkO9J3KrrxxiK21810aZnxOLZFuvgJ
2O/jugBK8MdCemBpmX93imgXSLJSXJf5/yVcETXFGUmf5p7Ze3Wdi0AxFvjPe2yN
D2MFvp2dtJ9mFaGaCG9v5wjyVVZM+oTGI9JzfNURq//qYWX+Tz9HJVNVeuYvEUnb
LXe9FVZej1+RVsBut9eCyo4GWOqMgRWp/dyMKz3shHFec0pc0fluo7yQH82OonoM
ZzkqgKVmkP5LVW0WqrDKbPTmpsq3ISYJwe7Msnu5D47iUnuc22axPzOH7ZRXE+2n
1Vkig4iYxz2IFZCwO3Ei9LxDlaJh+juHNnS0ziosDrTw0c/VWjkV+XwhRhfNq+Cx
crjMwThsIrz+JXrTihppMvSQhJHjIB/KoiUsa63qVsv6JA+yeBwvthwJ4Kl2ioDg
rH2VNKMU9e/dsWRqfRdUxH29pyJHQFjlv8MXWlFrKgoyrOLN2wkO2RJdKm0hblZW
vh+RH1AiwshFKw9rxdUXJBGGVgn5F0Ie4alDI8ehelOpmrZgFYMOCpcFSpJ5vbXM
ny6l9/duT2POAsUN5IwHGDu8b2NT+vCUQRFVHY31
-----END CERTIFICATE-----
)EOF";





WiFiClientSecure ssl = WiFiClientSecure();
Config config;
ConfigCoMCU configcomcu;
ThingsBoard tbProvision(ssl);
ThingsBoardSized<DOCSIZE, 64> tb(ssl);
volatile bool provisionResponseProcessed = false;
const Provision_Callback provisionCallback = processProvisionResponse;
bool FLAG_IOT_INIT = 0;
bool FLAG_OTA_UPDATE_INIT = 0;

void startup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, PIN_RXD2, PIN_TXD2);

  if(!SPIFFS.begin(true))
  {
    configReset();
    configLoadFailSafe();
    sprintf_P(logBuff, PSTR("Problem with file system. Failsafe config was loaded."));
    recordLog(1, PSTR(__FILE__), __LINE__, PSTR(__func__));
  }
  else
  {
    configLoad();
  }

  WiFi.onEvent(cbWifiOnConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(cbWiFiOnDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(cbWiFiOnLostIp, SYSTEM_EVENT_STA_LOST_IP);
  WiFi.onEvent(cbWiFiOnGotIp, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wssid, config.wpass);
  WiFi.setHostname(config.name);
  WiFi.setAutoReconnect(true);

  ssl.setCACert(NARIN_CERT_CA);
}

void udawa() {
  ArduinoOTA.handle();

  if (!provisionResponseProcessed) {
    tbProvision.loop();
  }
  if(config.accessToken && config.provSent)
  {
    tb.loop();
  }

  if(FLAG_IOT_INIT)
  {
    FLAG_IOT_INIT = 0;
    iotInit();
  }
  if(FLAG_OTA_UPDATE_INIT)
  {
    FLAG_OTA_UPDATE_INIT = 0;
    otaUpdateInit();
  }
}

void reboot()
{
  esp_task_wdt_init(1,true);
  esp_task_wdt_add(NULL);
  while(true);
}

char* getDeviceId()
{
  char* decodedString = new char[16];
  uint64_t chipid = ESP.getEfuseMac();
  sprintf(decodedString, "%04X%08X",(uint16_t)(chipid>>32), (uint32_t)chipid);
  return decodedString;
}

void otaUpdateInit()
{
  ArduinoOTA.setHostname(config.name);
  ArduinoOTA.setPasswordHash(config.upass);
  ArduinoOTA.begin();

  ArduinoOTA
    .onStart([]()
    {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
      else // U_SPIFFS
          type = "filesystem";
    })
    .onEnd([]()
    {
      reboot();
    })
    .onProgress([](unsigned int progress, unsigned int total)
    {

    })
    .onError([](ota_error_t error)
    {
      reboot();
    }
  );
}

void iotInit()
{
  if(!config.provSent)
  {
    if(!tbProvision.connected())
    {
      sprintf_P(logBuff, PSTR("Starting provision initiation to %s:%d"),  config.broker, config.port);
      recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
      if(!tbProvision.connect(config.broker, "provision", config.port))
      {
        sprintf_P(logBuff, PSTR("Failed to connect to provisioning server: %s:%d"),  config.broker, config.port);
        recordLog(1, PSTR(__FILE__), __LINE__, PSTR(__func__));
        return;
      }
      if(tbProvision.Provision_Subscribe(provisionCallback))
      {
        if(tbProvision.sendProvisionRequest(config.name, config.provisionDeviceKey, config.provisionDeviceSecret))
        {
          config.provSent = true;
          sprintf_P(logBuff, PSTR("Provision request was sent!"));
          recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
        }
      }
    }
  }
  else if(provisionResponseProcessed || config.accessToken)
  {
    if(!tb.connected())
    {
      sprintf_P(logBuff, PSTR("Connecting to broker %s:%d"), config.broker, config.port);
      recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
      if(!tb.connect(config.broker, config.accessToken, config.port))
      {
        sprintf_P(logBuff, PSTR("Failed to connect to IoT Broker %s"), config.broker);
        recordLog(1, PSTR(__FILE__), __LINE__, PSTR(__func__));
        return;
      }
      else
      {
        sprintf_P(logBuff, PSTR("IoT Connected!"));
        recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
        FLAG_IOT_RPC_SUBSCRIBE = true;
      }
    }
  }
}

void cbWifiOnConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  IPAddress ip = WiFi.localIP();
  char ipa[25];
  sprintf(ipa, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  sprintf_P(logBuff, PSTR("WiFi Connected to %s"), WiFi.SSID().c_str());
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
}

void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  sprintf_P(logBuff, PSTR("WiFi (%s) Disconnected!"), WiFi.SSID().c_str());
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  WiFi.reconnect();
}

void cbWiFiOnLostIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  sprintf_P(logBuff, PSTR("WiFi (%s) IP Lost!"), WiFi.SSID().c_str());
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  WiFi.reconnect();
}

void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  FLAG_IOT_INIT = 1;
  FLAG_OTA_UPDATE_INIT = 1;
}

void configReset()
{
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(configFile, FILE_WRITE);
  if (!file) {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  char dv[16];
  sprintf(dv, "%s", getDeviceId());
  doc["name"] = "UDAWA" + String(dv);
  doc["model"] = "Generic";
  doc["group"] = "PRITA";
  doc["broker"] = "prita.undiknas.ac.id";
  doc["port"] = 8883;
  doc["wssid"] = wssid;
  doc["wpass"] = wpass;
  doc["upass"] = upass;
  doc["accessToken"] = accessToken;
  doc["provSent"] = false;
  doc["provisionDeviceKey"] = provisionDeviceKey;
  doc["provisionDeviceSecret"] = provisionDeviceSecret;
  doc["logLev"] = 5;

  serializeJson(doc, file);
  file.close();

  sprintf_P(logBuff, PSTR("Config hard reset:"));
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  serializeJsonPretty(doc, Serial);
}

void configLoadFailSafe()
{
  char dv[16];
  sprintf(dv, "%s", getDeviceId());
  strlcpy(config.hwid, dv, sizeof(config.hwid));

  String name = "UDAWA" + String(dv);
  strlcpy(config.name, name.c_str(), sizeof(config.name));
  strlcpy(config.model, "Generic", sizeof(config.model));
  strlcpy(config.group, "PRITA", sizeof(config.group));
  strlcpy(config.broker, "prita.undiknas.ac.id", sizeof(config.broker));
  strlcpy(config.wssid, wssid, sizeof(config.wssid));
  strlcpy(config.wpass, wpass, sizeof(config.wpass));
  strlcpy(config.upass, upass, sizeof(config.upass));
  strlcpy(config.accessToken, accessToken, sizeof(config.accessToken));
  config.provSent = false;
  config.port = 8883;
  strlcpy(config.provisionDeviceKey, provisionDeviceKey, sizeof(config.provisionDeviceKey));
  strlcpy(config.provisionDeviceSecret, provisionDeviceSecret, sizeof(config.provisionDeviceSecret));
  config.logLev = 5;
}

void configLoad()
{
  File file = SPIFFS.open(configFile);
  StaticJsonDocument<DOCSIZE> doc;
  DeserializationError error = deserializeJson(doc, file);

  if(error)
  {
    file.close();
    configReset();
    return;
  }
  else
  {
    char dv[16];
    sprintf(dv, "%s", getDeviceId());
    strlcpy(config.hwid, dv, sizeof(config.hwid));

    sprintf_P(logBuff, PSTR("Device ID: %s"), dv);
    recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));

    strlcpy(config.name, doc["name"].as<const char*>(), sizeof(config.name));
    strlcpy(config.model, doc["model"].as<const char*>(), sizeof(config.model));
    strlcpy(config.group, doc["group"].as<const char*>(), sizeof(config.group));
    strlcpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker));
    strlcpy(config.wssid, doc["wssid"].as<const char*>(), sizeof(config.wssid));
    strlcpy(config.wpass, doc["wpass"].as<const char*>(), sizeof(config.wpass));
    strlcpy(config.upass, doc["upass"].as<const char*>(), sizeof(config.upass));
    strlcpy(config.accessToken, doc["accessToken"].as<const char*>(), sizeof(config.accessToken));
    config.provSent = doc["provSent"].as<bool>();
    config.port = doc["port"].as<uint16_t>() ? doc["port"].as<uint16_t>() : 8883;
    config.logLev = doc["logLev"].as<uint8_t>();
    strlcpy(config.provisionDeviceKey, doc["provisionDeviceKey"].as<const char*>(), sizeof(config.provisionDeviceKey));
    strlcpy(config.provisionDeviceSecret, doc["provisionDeviceSecret"].as<const char*>(), sizeof(config.provisionDeviceSecret));

    sprintf_P(logBuff, PSTR("Config loaded successfuly."));
    recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  }
  file.close();
}

void configSave()
{
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(configFile, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;
  doc["name"] = config.name;
  doc["model"] = config.model;
  doc["group"] = config.group;
  doc["broker"] = config.broker;
  doc["port"]  = config.port;
  doc["wssid"] = config.wssid;
  doc["wpass"] = config.wpass;
  doc["upass"] = config.upass;
  doc["accessToken"] = config.accessToken;
  doc["provSent"] = config.provSent;
  doc["provisionDeviceKey"] = config.provisionDeviceKey;
  doc["provisionDeviceSecret"] = config.provisionDeviceSecret;
  doc["logLev"] = config.logLev;

  serializeJson(doc, file);
  file.close();

}


void configCoMCUReset()
{
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
  if (!file) {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  doc["fPanic"] = false;
  doc["pEcKcoe"] = 2.9;
  doc["pEcTcoe"] = 0.019;
  doc["pEcVin"] = 4.54;
  doc["pEcPpm"] = 0.5;
  doc["pEcR1"] = 992;
  doc["pEcRa"] =  25;

  doc["bfreq"] = 1600;
  doc["fBuzz"] = 1;

  doc["pinBuzzer"] = 3;
  doc["pinLedR"] = 9;
  doc["pinLedG"] = 10;
  doc["pinLedB"] = 11;
  doc["pinEcPower"] = 15;
  doc["pinEcGnd"] = 16;
  doc["pinEcData"] = 14;

  serializeJson(doc, file);
  file.close();

  sprintf_P(logBuff, PSTR("ConfigCoMCU hard reset:"));
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  serializeJsonPretty(doc, Serial);
}

void configCoMCULoad()
{
  File file = SPIFFS.open(configFileCoMCU);
  StaticJsonDocument<DOCSIZE> doc;
  DeserializationError error = deserializeJson(doc, file);

  if(error)
  {
    file.close();
    configCoMCUReset();
    return;
  }
  else
  {

    configcomcu.fPanic = doc["fPanic"].as<bool>();
    configcomcu.pEcKcoe = doc["pEcKcoe"].as<float>();
    configcomcu.pEcTcoe = doc["pEcTcoe"].as<float>();
    configcomcu.pEcVin = doc["pEcVin"].as<float>();
    configcomcu.pEcPpm = doc["pEcPpm"].as<float>();
    configcomcu.pEcR1 = doc["pEcR1"].as<uint16_t>();
    configcomcu.pEcRa =  doc["pEcRa"].as<uint16_t>();

    configcomcu.bfreq = doc["bfreq"].as<uint16_t>();
    configcomcu.fBuzz = doc["fBuzz"].as<bool>();

    configcomcu.pinBuzzer = doc["pinBuzzer"].as<uint8_t>();
    configcomcu.pinLedR = doc["pinLedR"].as<uint8_t>();
    configcomcu.pinLedG = doc["pinLedG"].as<uint8_t>();
    configcomcu.pinLedB = doc["pinLedB"].as<uint8_t>();
    configcomcu.pinEcPower = doc["pinEcPower"].as<uint8_t>();
    configcomcu.pinEcGnd = doc["pinEcGnd"].as<uint8_t>();
    configcomcu.pinEcData = doc["pinEcData"].as<uint8_t>();

    sprintf_P(logBuff, PSTR("ConfigCoMCU loaded successfuly."));
    recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  }
  file.close();
}

void configCoMCUSave()
{
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  doc["fPanic"] = configcomcu.fPanic;
  doc["pEcKcoe"] = configcomcu.pEcKcoe;
  doc["pEcTcoe"] = configcomcu.pEcTcoe;
  doc["pEcVin"] = configcomcu.pEcVin;
  doc["pEcPpm"] = configcomcu.pEcPpm;
  doc["pEcR1"] = configcomcu.pEcPpm;
  doc["pEcRa"] = configcomcu.pEcRa;

  doc["bfreq"] = configcomcu.bfreq;
  doc["fBuzz"] = configcomcu.fBuzz;

  doc["pinBuzzer"] = configcomcu.pinBuzzer;
  doc["pinLedR"] = configcomcu.pinLedR;
  doc["pinLedG"] = configcomcu.pinLedG;
  doc["pinLedB"] = configcomcu.pinLedB;
  doc["pinEcPower"] = configcomcu.pinEcPower;
  doc["pinEcGnd"] = configcomcu.pinEcGnd;
  doc["pinEcData"] = configcomcu.pinEcData;

  serializeJson(doc, file);
  file.close();

}

void syncConfigCoMCU()
{
  configCoMCULoad();

  StaticJsonDocument<DOCSIZE> doc;
  doc["fPanic"] = configcomcu.fPanic;
  doc["pEcKcoe"] = configcomcu.pEcKcoe;
  doc["pEcTcoe"] = configcomcu.pEcTcoe;
  doc["pEcVin"] = configcomcu.pEcVin;
  doc["pEcPpm"] = configcomcu.pEcPpm;
  doc["pEcR1"] = configcomcu.pEcPpm;
  doc["pEcRa"] = configcomcu.pEcRa;

  doc["bfreq"] = configcomcu.bfreq;
  doc["fBuzz"] = configcomcu.fBuzz;

  doc["pinBuzzer"] = configcomcu.pinBuzzer;
  doc["pinLedR"] = configcomcu.pinLedR;
  doc["pinLedG"] = configcomcu.pinLedG;
  doc["pinLedB"] = configcomcu.pinLedB;
  doc["pinEcPower"] = configcomcu.pinEcPower;
  doc["pinEcGnd"] = configcomcu.pinEcGnd;
  doc["pinEcData"] = configcomcu.pinEcData;

  doc["method"] = "setConfigCoMCU";
  serialWriteToCoMcu(doc, 0);
}

bool loadFile(const char* filePath, char *buffer)
{
  File file = SPIFFS.open(filePath);
  if(!file || file.isDirectory()){
      file.close();
      return false;
  }
  uint16_t i = 0;
  while(file.available()){
    buffer[i] = file.read();
    i++;
  }
  buffer[i] = '\0';
  file.close();
  return true;
}

void processProvisionResponse(const Provision_Data &data)
{
  sprintf_P(logBuff, PSTR("Received device provision response"));
  recordLog(4, PSTR(__FILE__), __LINE__, PSTR(__func__));
  int jsonSize = measureJson(data) + 1;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0)
  {
    sprintf_P(logBuff, PSTR("Provision response contains the error: %s"), data["errorMsg"].as<const char*>());
    recordLog(1, PSTR(__FILE__), __LINE__, PSTR(__func__));
    provisionResponseProcessed = true;
    return;
  }
  else
  {
    sprintf_P(logBuff, PSTR("Provision response credential type: %s"), data["credentialsType"].as<const char*>());
    recordLog(1, PSTR(__FILE__), __LINE__, PSTR(__func__));
  }
  if (strncmp(data["credentialsType"], "ACCESS_TOKEN", strlen("ACCESS_TOKEN")) == 0)
  {
    strlcpy(config.accessToken, data["credentialsValue"].as<String>().c_str(), sizeof(config.accessToken));
    configSave();
    iotInit();
    FLAG_IOT_RPC_SUBSCRIBE = true;
  }
  if (strncmp(data["credentialsType"], "MQTT_BASIC", strlen("MQTT_BASIC")) == 0)
  {
    /*JsonObject credentials_value = data["credentialsValue"].as<JsonObject>();
    credentials.client_id = credentials_value["clientId"].as<String>();
    credentials.username = credentials_value["userName"].as<String>();
    credentials.password = credentials_value["password"].as<String>();
    */
  }
  if (tbProvision.connected()) {
    tbProvision.disconnect();
  }
  provisionResponseProcessed = true;
}

void recordLog(uint8_t level, const char* fileName, int lineNumber, const char* functionName)
{
  if(_logRecIndex == LOG_REC_SIZE)
  {
    _logRecIndex = 0;
  }
  const char *levels;
  if(level == 5){levels = "D";}
  else if(level == 4){levels = "I";}
  else if(level == 3){levels = "W";}
  else if(level == 2){levels = "C";}
  else if(level == 1){levels = "E";}
  else{levels = "X";}
  sprintf_P(_logRec[_logRecIndex], PSTR("[%s][%s:%d] %s: %s"), levels, fileName, lineNumber, functionName, logBuff);
  if(level <= config.logLev)
  {
    Serial.println(_logRec[_logRecIndex]);
  }
  _logRecIndex++;
}

void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE> &doc, bool isRpc)
{
  if(false)
  {
    StringPrint stream;
    serializeJson(doc, stream);
    String result = stream.str();
    sprintf_P(logBuff, PSTR("%s"), result.c_str());
    recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
  }
  serializeJson(doc, Serial2);
  serializeJsonPretty(doc, Serial);
  if(isRpc)
  {
    delay(50);
    serialReadFromCoMcu(doc);
  }
}

void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE> &doc)
{
  StringPrint stream;
  String result;
  ReadLoggingStream loggingStream(Serial2, stream);
  DeserializationError err = deserializeJson(doc, loggingStream);
  result = stream.str();
  if (err == DeserializationError::Ok)
  {
    if(true){
      sprintf_P(logBuff, PSTR("%s"), result.c_str());
      recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
    }
  }
  else
  {
    sprintf_P(logBuff, PSTR("Serial2CoMCU DeserializeJson() returned: %s, content: %s"), err.c_str(), result.c_str());
    recordLog(5, PSTR(__FILE__), __LINE__, PSTR(__func__));
    return;
  }
}


void readSettings(StaticJsonDocument<DOCSIZE> &doc, const char* path)
{
  File file = SPIFFS.open(path);
  DeserializationError error = deserializeJson(doc, file);

  if(error)
  {
    file.close();
    return;
  }
  file.close();
}

void writeSettings(StaticJsonDocument<DOCSIZE> &doc, const char* path)
{
  SPIFFS.remove(configFile);
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }
  serializeJson(doc, file);
  file.close();
}

#endif