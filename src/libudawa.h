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
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <ArduinoOTA.h>
#include <thingsboard.h>
#include <TaskManagerIO.h>
#include "logging.h"
#include "serialLogger.h"
#include <ESP32Time.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define COMPILED __DATE__ " " __TIME__
#define LOG_REC_SIZE 10
#define LOG_REC_LENGTH 192
#define PIN_RXD2 16
#define PIN_TXD2 17
#define WIFI_FALLBACK_COUNTER 5
#ifndef DOCSIZE
  #define DOCSIZE 1024
#endif

namespace libudawa
{

const char* configFile = "/cfg.json";
const char* configFileCoMCU = "/comcu.json";
bool FLAG_IOT_SUBSCRIBE = false;
bool FLAG_IOT_INIT = false;
bool FLAG_OTA_UPDATE_INIT = false;
uint8_t WIFI_RECONNECT_ATTEMPT = 0;
uint8_t IOT_RECONNECT_ATTEMPT = 0;
bool WIFI_IS_DEFAULT = false;

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
  char dssid[64];
  char dpass[64];
  char upass[64];
  char accessToken[64];
  bool provSent;

  char provisionDeviceKey[24];
  char provisionDeviceSecret[24];

  int gmtOffset;
};

struct ConfigCoMCU
{
  bool fPanic;
  uint16_t bfreq;
  bool fBuzz;
  uint8_t pinBuzzer;
  uint8_t pinLedR;
  uint8_t pinLedG;
  uint8_t pinLedB;
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
callbackResponse processProvisionResponse(const callbackData &data);
void iotInit();
void startup();
void networkInit();
void udawa();
void otaUpdateInit();
void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE> &doc, bool isRpc);
void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE> &doc);
void syncConfigCoMCU();
void readSettings(StaticJsonDocument<DOCSIZE> &doc,const char* path);
void writeSettings(StaticJsonDocument<DOCSIZE> &doc, const char* path);
void setCoMCUPin(uint8_t pin, char type, bool mode, uint16_t aval, bool state);
void rtcUpdate(long ts = 0);

ESP32SerialLogger serial_logger;
LogManager *log_manager = LogManager::GetInstance(LogLevel::VERBOSE);
WiFiClientSecure ssl = WiFiClientSecure();
Config config;
ConfigCoMCU configcomcu;
ThingsBoardSized<DOCSIZE, 64, LogManager> tb(ssl);
volatile bool provisionResponseProcessed = false;
ESP32Time rtc(28800);


void rtcUpdate(long ts){
  if(ts == 0){
    configTime(config.gmtOffset, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)){
      rtc.setTimeStruct(timeinfo);
      log_manager->debug(PSTR(__func__), "Updated time via NTP: %s GMT Offset:%d (%d) \n", rtc.getDateTime().c_str(), config.gmtOffset, config.gmtOffset / 3600);
    }
  }else{
      rtc.setTime(ts);
      log_manager->debug(PSTR(__func__), "Updated time via timestamp: %s\n", rtc.getDateTime().c_str());
  }
}

void startup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  #ifdef USE_SERIAL2
    Serial2.begin(115200, SERIAL_8N1, PIN_RXD2, PIN_TXD2);
  #endif

  config.logLev = 5;
  log_manager->add_logger(&serial_logger);
  if(!SPIFFS.begin(true))
  {
    //configReset();
    configLoadFailSafe();
    log_manager->error(PSTR(__func__), PSTR("Problem with file system. Failsafe config was loaded.\n"));
  }
  else
  {
    log_manager->info(PSTR(__func__), PSTR("Loading config...\n"));
    configLoad();
    log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);
  }

  log_manager->debug(PSTR(__func__), "Startup time: %s\n", rtc.getDateTime().c_str());
}

void networkInit()
{
  WiFi.onEvent(cbWifiOnConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(cbWiFiOnDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(cbWiFiOnLostIp, ARDUINO_EVENT_WIFI_STA_LOST_IP);
  WiFi.onEvent(cbWiFiOnGotIp, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.mode(WIFI_STA);
  if(!config.wssid || *config.wssid == 0x00 || strlen(config.wssid) > 32)
  {
    configLoadFailSafe();
    log_manager->error(PSTR(__func__), PSTR("SSID too long or missing! Failsafe config was loaded.\n"));
  }
  WiFi.begin(config.wssid, config.wpass);
  WiFi.setHostname(config.name);
  WiFi.setAutoReconnect(true);

  ssl.setCACert(CA_CERT);

  taskManager.scheduleFixedRate(10000, [] {
    if(WiFi.status() == WL_CONNECTED && !tb.connected())
    {
      iotInit();
    }
  });

  unsigned long otaTimer = millis();
  while(true)
  {
    ArduinoOTA.handle();
    if(millis() - otaTimer > 10000)
    {
      break;
    }
    delay(10);
  }
}

void udawa() {
  taskManager.runLoop();
  ArduinoOTA.handle();

  tb.loop();

  if(FLAG_OTA_UPDATE_INIT)
  {
    FLAG_OTA_UPDATE_INIT = 0;
    otaUpdateInit();
  }

  if(FLAG_IOT_INIT)
  {
    FLAG_IOT_INIT = 0;
    iotInit();
  }
}

void reboot()
{
  log_manager->info(PSTR(__func__),PSTR("Device rebooting...\n"));
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
  int freeHeap = ESP.getFreeHeap();
  log_manager->info(PSTR(__func__),PSTR("Initializing IoT, available memory: %d\n"), freeHeap);
  if(freeHeap < 92000)
  {
    log_manager->error(PSTR(__func__),PSTR("Unable to init IoT, insufficient memory: %d\n"), freeHeap);
    return;
  }
  if(!config.provSent)
  {
    ThingsBoardSized<DOCSIZE, 64> tbProvision(ssl);
    if(!tbProvision.connected())
    {
      log_manager->info(PSTR(__func__),PSTR("Starting provision initiation to %s:%d\n"),  config.broker, config.port);
      if(tbProvision.connect(config.broker, "provision", config.port))
      {
        log_manager->info(PSTR(__func__),PSTR("Connected to provisioning server: %s:%d\n"),  config.broker, config.port);

        GenericCallback cb[2] = {
          { "provisionResponse", processProvisionResponse },
          { "provisionResponse", processProvisionResponse }
        };
        if(tbProvision.callbackSubscribe(cb, 2))
        {
          if(tbProvision.sendProvisionRequest(config.name, config.provisionDeviceKey, config.provisionDeviceSecret))
          {
            log_manager->info(PSTR(__func__),PSTR("Provision request was sent! Waiting for response.\n"));
            unsigned long timer = millis();
            while(true)
            {
              tbProvision.loop();
              if(millis() - timer > 10000)
              {
                break;
              }
            }
            tbProvision.disconnect();
          }
        }
      }
      else
      {
        log_manager->error(PSTR(__func__),PSTR("Failed to connect to provisioning server: %s:%d\n"),  config.broker, config.port);
        return;
      }
    }
  }
  else if(config.provSent)
  {
    if(!tb.connected())
    {
      log_manager->info(PSTR(__func__),PSTR("Connecting to broker %s:%d\n"), config.broker, config.port);
      if(!tb.connect(config.broker, config.accessToken, config.port, config.name))
      {
        log_manager->error(PSTR(__func__),PSTR("Failed to connect to IoT Broker %s [%d/20]\n"), config.broker, IOT_RECONNECT_ATTEMPT);
        IOT_RECONNECT_ATTEMPT++;
        if(IOT_RECONNECT_ATTEMPT >= 20){
          config.provSent= 0;
        }
        return;
      }

      log_manager->info(PSTR(__func__),PSTR("IoT Connected!\n"));
      IOT_RECONNECT_ATTEMPT = 0;
      FLAG_IOT_SUBSCRIBE = true;
    }
  }
}

void cbWifiOnConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->info(PSTR(__func__),PSTR("WiFi Connected to %s\n"), WiFi.SSID().c_str());
  WIFI_RECONNECT_ATTEMPT = 0;
}

void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  WIFI_RECONNECT_ATTEMPT += 1;
  if(WIFI_RECONNECT_ATTEMPT >= WIFI_FALLBACK_COUNTER)
  {
    if(!WIFI_IS_DEFAULT)
    {
      log_manager->error(PSTR(__func__),PSTR("WiFi (%s) Disconnected! Attempt: %d/%d\n"), config.dssid, WIFI_RECONNECT_ATTEMPT, WIFI_FALLBACK_COUNTER);
      WiFi.begin(config.dssid, config.dpass);
      WIFI_IS_DEFAULT = true;
    }
    else
    {
      log_manager->info(PSTR(__func__),PSTR("WiFi (%s) Disconnected! Attempt: %d/%d\n"), config.wssid, WIFI_RECONNECT_ATTEMPT, WIFI_FALLBACK_COUNTER);
      WiFi.begin(config.wssid, config.wpass);
      WIFI_IS_DEFAULT = false;
    }
    WIFI_RECONNECT_ATTEMPT = 0;
  }
  else
  {
    WiFi.reconnect();
  }
}

void cbWiFiOnLostIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->error(PSTR(__func__),PSTR("WiFi (%s) IP Lost!\n"), WiFi.SSID().c_str());
  WiFi.reconnect();
}

void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  FLAG_OTA_UPDATE_INIT = 1;
  FLAG_IOT_INIT = 1;
  rtcUpdate(0);
}

void configReset()
{
  bool formatted = SPIFFS.format();
  if(formatted)
  {
    log_manager->info(PSTR(__func__),PSTR("SPIFFS formatting success.\n"));
  }
  else
  {
    log_manager->error(PSTR(__func__),PSTR("SPIFFS formatting failed.\n"));
  }
  File file;
  file = SPIFFS.open(configFile, FILE_WRITE);
  if (!file) {
    log_manager->error(PSTR(__func__),PSTR("Failed to create config file. Config reset is cancelled.\n"));
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  char dv[16];
  sprintf(dv, "%s", getDeviceId());
  doc["name"] = "UDAWA" + String(dv);
  doc["model"] = "Generic";
  doc["group"] = "UDAWA";
  doc["broker"] = broker;
  doc["port"] = port;
  doc["wssid"] = wssid;
  doc["wpass"] = wpass;
  doc["dssid"] = dssid;
  doc["dpass"] = dpass;
  doc["upass"] = upass;
  doc["accessToken"] = accessToken;
  doc["provSent"] = false;
  doc["provisionDeviceKey"] = provisionDeviceKey;
  doc["provisionDeviceSecret"] = provisionDeviceSecret;
  doc["logLev"] = 5;
  doc["gmtOffset"] = 28880;

  size_t size = serializeJson(doc, file);
  file.close();

  log_manager->info(PSTR(__func__),PSTR("Resetted config file (size: %d) is written successfully...\n"), size);
  file = SPIFFS.open(configFile, FILE_READ);
  if (!file)
  {
    log_manager->error(PSTR(__func__),PSTR("Failed to open the config file!"));
  }
  else
  {
    log_manager->info(PSTR(__func__),PSTR("New config file opened, size: %d\n"), file.size());

    if(file.size() < 1)
    {
      log_manager->error(PSTR(__func__),PSTR("Config file size is abnormal: %d, trying to rewrite...\n"), file.size());

      size_t size = serializeJson(doc, file);
      log_manager->info(PSTR(__func__),PSTR("Writing: %d of data, file size: %d\n"), size, file.size());
    }
    else
    {
      log_manager->info(PSTR(__func__),PSTR("Config file size is normal: %d, trying to reboot...\n"), file.size());
      file.close();
      reboot();
    }
  }
  file.close();
}

void configLoadFailSafe()
{
  char dv[16];
  sprintf(dv, "%s", getDeviceId());
  strlcpy(config.hwid, dv, sizeof(config.hwid));

  String name = "UDAWA" + String(dv);
  strlcpy(config.name, name.c_str(), sizeof(config.name));
  strlcpy(config.model, "Generic", sizeof(config.model));
  strlcpy(config.group, "UDAWA", sizeof(config.group));
  strlcpy(config.broker, broker, sizeof(config.broker));
  strlcpy(config.wssid, wssid, sizeof(config.wssid));
  strlcpy(config.wpass, wpass, sizeof(config.wpass));
  strlcpy(config.dssid, dssid, sizeof(config.dssid));
  strlcpy(config.dpass, dpass, sizeof(config.dpass));
  strlcpy(config.upass, upass, sizeof(config.upass));
  strlcpy(config.accessToken, accessToken, sizeof(config.accessToken));
  config.provSent = false;
  config.port = port;
  strlcpy(config.provisionDeviceKey, provisionDeviceKey, sizeof(config.provisionDeviceKey));
  strlcpy(config.provisionDeviceSecret, provisionDeviceSecret, sizeof(config.provisionDeviceSecret));
  config.logLev = 5;
  config.gmtOffset = 28800;
}

void configLoad()
{
  File file = SPIFFS.open(configFile, FILE_READ);
  log_manager->info(PSTR(__func__),PSTR("Loading config file.\n"));
  if(file.size() > 1)
  {
    log_manager->info(PSTR(__func__),PSTR("Config file size is normal: %d, trying to fit it in %d docsize.\n"), file.size(), DOCSIZE);
  }
  else
  {
    file.close();
    log_manager->error(PSTR(__func__),PSTR("Config file size is abnormal: %d. Closing file and trying to reset...\n"), file.size());
    configReset();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;
  DeserializationError error = deserializeJson(doc, file);

  if(error)
  {
    log_manager->error(PSTR(__func__),PSTR("Failed to load config file! (%s - %s - %d). Falling back to failsafe.\n"), configFile, error.c_str(), file.size());
    file.close();
    configLoadFailSafe();
    return;
  }
  else
  {
    char dv[16];
    sprintf(dv, "%s", getDeviceId());
    strlcpy(config.hwid, dv, sizeof(config.hwid));

    log_manager->info(PSTR(__func__),PSTR("Device ID: %s\n"), dv);


    String name = "UDAWA" + String(dv);
    strlcpy(config.name, name.c_str(), sizeof(config.name));
    //strlcpy(config.name, doc["name"].as<const char*>(), sizeof(config.name));
    strlcpy(config.model, doc["model"].as<const char*>(), sizeof(config.model));
    strlcpy(config.group, doc["group"].as<const char*>(), sizeof(config.group));
    strlcpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker));
    strlcpy(config.wssid, doc["wssid"].as<const char*>(), sizeof(config.wssid));
    strlcpy(config.wpass, doc["wpass"].as<const char*>(), sizeof(config.wpass));
    strlcpy(config.dssid, doc["dssid"].as<const char*>(), sizeof(config.dssid));
    strlcpy(config.dpass, doc["dpass"].as<const char*>(), sizeof(config.dpass));
    strlcpy(config.upass, doc["upass"].as<const char*>(), sizeof(config.upass));
    strlcpy(config.accessToken, doc["accessToken"].as<const char*>(), sizeof(config.accessToken));
    config.provSent = doc["provSent"].as<int>();
    config.port = doc["port"].as<uint16_t>() ? doc["port"].as<uint16_t>() : port;
    config.logLev = doc["logLev"].as<uint8_t>();
    config.gmtOffset = doc["gmtOffset"].as<int>();
    strlcpy(config.provisionDeviceKey, doc["provisionDeviceKey"].as<const char*>(), sizeof(config.provisionDeviceKey));
    strlcpy(config.provisionDeviceSecret, doc["provisionDeviceSecret"].as<const char*>(), sizeof(config.provisionDeviceSecret));

    log_manager->info(PSTR(__func__),PSTR("Config loaded successfuly.\n"));
  }
  file.close();
}

void configSave()
{
  if(!SPIFFS.remove(configFile))
  {
    log_manager->error(PSTR(__func__),PSTR("Failed to delete the old configFile: %s\n"), configFile);
  }
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
  doc["dssid"] = config.dssid;
  doc["dpass"] = config.dpass;
  doc["upass"] = config.upass;
  doc["accessToken"] = config.accessToken;
  doc["provSent"] = config.provSent;
  doc["provisionDeviceKey"] = config.provisionDeviceKey;
  doc["provisionDeviceSecret"] = config.provisionDeviceSecret;
  doc["logLev"] = config.logLev;
  doc["gmtOffset"] = config.gmtOffset;

  serializeJson(doc, file);
  file.close();
}


void configCoMCUReset()
{
  SPIFFS.remove(configFileCoMCU);
  File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
  if (!file) {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  doc["fPanic"] = false;

  doc["bfreq"] = 1600;
  doc["fBuzz"] = 1;

  doc["pinBuzzer"] = 3;
  doc["pinLedR"] = 9;
  doc["pinLedG"] = 10;
  doc["pinLedB"] = 11;

  serializeJson(doc, file);
  file.close();

  log_manager->info(PSTR(__func__),PSTR("ConfigCoMCU hard reset:"));
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
    configcomcu.bfreq = doc["bfreq"].as<uint16_t>();
    configcomcu.fBuzz = doc["fBuzz"].as<bool>();

    configcomcu.pinBuzzer = doc["pinBuzzer"].as<uint8_t>();
    configcomcu.pinLedR = doc["pinLedR"].as<uint8_t>();
    configcomcu.pinLedG = doc["pinLedG"].as<uint8_t>();
    configcomcu.pinLedB = doc["pinLedB"].as<uint8_t>();

    log_manager->info(PSTR(__func__),PSTR("ConfigCoMCU loaded successfuly.\n"));
  }
  file.close();
}

void configCoMCUSave()
{
  if(!SPIFFS.remove(configFileCoMCU))
  {
    log_manager->error(PSTR(__func__),PSTR("Failed to delete the old configFileCoMCU: %s\n"), configFileCoMCU);
  }
  File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  doc["fPanic"] = configcomcu.fPanic;

  doc["bfreq"] = configcomcu.bfreq;
  doc["fBuzz"] = configcomcu.fBuzz;

  doc["pinBuzzer"] = configcomcu.pinBuzzer;
  doc["pinLedR"] = configcomcu.pinLedR;
  doc["pinLedG"] = configcomcu.pinLedG;
  doc["pinLedB"] = configcomcu.pinLedB;

  serializeJson(doc, file);
  file.close();
}

void syncConfigCoMCU()
{
  configCoMCULoad();

  StaticJsonDocument<DOCSIZE> doc;
  doc["fPanic"] = configcomcu.fPanic;
  doc["bfreq"] = configcomcu.bfreq;
  doc["fBuzz"] = configcomcu.fBuzz;
  doc["pinBuzzer"] = configcomcu.pinBuzzer;
  doc["pinLedR"] = configcomcu.pinLedR;
  doc["pinLedG"] = configcomcu.pinLedG;
  doc["pinLedB"] = configcomcu.pinLedB;
  doc["method"] = "setConfigCoMCU";
  serialWriteToCoMcu(doc, 0);
  doc.clear();
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

callbackResponse processProvisionResponse(const callbackData &data)
{
  log_manager->info(PSTR(__func__),PSTR("Received device provision response\n"));
  int jsonSize = measureJson(data) + 1;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0)
  {
    log_manager->error(PSTR(__func__),PSTR("Provision response contains the error: %s\n"), data["errorMsg"].as<const char*>());
    provisionResponseProcessed = true;
    return callbackResponse("provisionResponse", 1);
  }
  else
  {
    log_manager->info(PSTR(__func__),PSTR("Provision response credential type: %s\n"), data["credentialsType"].as<const char*>());
  }
  if (strncmp(data["credentialsType"], "ACCESS_TOKEN", strlen("ACCESS_TOKEN")) == 0)
  {
    log_manager->info(PSTR(__func__),PSTR("ACCESS TOKEN received: %s\n"), data["credentialsValue"].as<String>().c_str());
    strlcpy(config.accessToken, data["credentialsValue"].as<String>().c_str(), sizeof(config.accessToken));
    config.provSent = true;
    configSave();
  }
  if (strncmp(data["credentialsType"], "MQTT_BASIC", strlen("MQTT_BASIC")) == 0)
  {
    /*JsonObject credentials_value = data["credentialsValue"].as<JsonObject>();
    credentials.client_id = credentials_value["clientId"].as<String>();
    credentials.username = credentials_value["userName"].as<String>();
    credentials.password = credentials_value["password"].as<String>();
    */
  }
  provisionResponseProcessed = true;
  return callbackResponse("provisionResponse", 1);
  reboot();
}

void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE> &doc, bool isRpc)
{
  serializeJson(doc, Serial2);
  StringPrint stream;
  serializeJsonPretty(doc, stream);
  String result = stream.str();
  log_manager->debug(PSTR(__func__),PSTR("Sent to CoMCU:\n%s\n"), result.c_str());
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
    log_manager->debug(PSTR(__func__),PSTR("Received from CoMCU\n%s\n"), result.c_str());
  }
  else
  {
    log_manager->debug(PSTR(__func__),PSTR("Serial2CoMCU DeserializeJson() returned: %s, content: %s\n"), err.c_str(), result.c_str());
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
  SPIFFS.remove(path);
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }
  serializeJson(doc, file);
  file.close();
}

void setCoMCUPin(uint8_t pin, char type, bool mode, uint16_t aval, bool state)
{
  StaticJsonDocument<DOCSIZE> doc;
  JsonObject params = doc.createNestedObject("params");
  doc["method"] = "setPin";
  params["pin"] = pin;
  params["mode"] = mode;
  params["type"] = type;
  params["state"] = state;
  params["aval"] = aval;
  serialWriteToCoMcu(doc, false);
}

} // namespace libudawa
#endif
