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
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <ArduinoOTA.h>
#include <thingsboard.h>
#include "logging.h"
#include "serialLogger.h"
#include <ESP32Time.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#define DISABLE_ALL_LIBRARY_WARNINGS
#include <esp32FOTA.hpp>
#include <WiFiUdp.h>

#define _TASK_TIMECRITICAL
#define _TASK_STATUS_REQUEST
#define _TASK_LTS_POINTER
#define _TASK_TIMEOUT
#define _TASK_EXPOSE_CHAIN
#define _TASK_SELF_DESTRUCT
#include <TaskScheduler.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define COMPILED __DATE__ " " __TIME__
#define S2_RX 16
#define S2_TX 17
#define WIFI_FALLBACK_COUNTER 3
#ifndef DOCSIZE
  #define DOCSIZE 1024
#endif

#ifndef DOCSIZE_MIN
  #define DOCSIZE_MIN 384
#endif


namespace libudawa
{

const char* configFile = "/cfg.json";
const char* configFileCoMCU = "/comcu.json";
bool FLAG_IOT_SUBSCRIBE = false;
bool FLAG_IOT_INIT = false;
bool FLAG_OTA_UPDATE_INIT = false;
bool WIFI_IS_DEFAULT = false;

struct Config
{
  char hwid[16];
  char name[24];
  char model[16];
  char group[16];
  uint8_t logLev;

  char broker[48];
  uint16_t port;
  char wssid[24];
  char wpass[24];
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
  bool fIface;
  char hname[40];
  char htU[24];
  char htP[24];

  uint8_t tbDisco = 0;

  char logIP[16] = "192.168.18.255";
  uint16_t logPrt = 29514;
};

struct ConfigCoMCU
{
  bool fP;
  uint16_t bFr;
  bool fB;
  uint8_t pBz;
  uint8_t pLR;
  uint8_t pLG;
  uint8_t pLB;
  uint8_t lON;
};

class GCB {
  public:
    using processFn = JsonObject (*)(JsonObject &data);

    inline GCB()
      : m_name(), m_cb(NULL)                {  }

    inline GCB(const char *methodName, processFn cb)
      : m_name(methodName), m_cb(cb)        {  }

    const char  *m_name;
    processFn   m_cb;
};

class ESP32UDPLogger : public ILogHandler
{
    public:
        void log_message(const char *tag, const LogLevel level, const char *fmt, va_list args) override;
};

uint32_t micro2milli(uint32_t hi, uint32_t lo);
double round2(double value);
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
void startup();
bool wifiKeeperEnable();
void wifiKeeperCb();
void udawa();
void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, bool isRpc);
void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc);
void syncConfigCoMCU();
void readSettings(StaticJsonDocument<DOCSIZE> &doc,const char* path);
void writeSettings(StaticJsonDocument<DOCSIZE> &doc, const char* path);
void setCoMCUPin(uint8_t pin, uint8_t op, uint8_t mode, uint16_t aval, uint8_t state);
void rtcUpdate(long ts = 0);
void setBuzzer(int32_t beepCount, uint16_t beepDelay);
void setLed(uint8_t r, uint8_t g, uint8_t b, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay);
void setLed(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay);
void setAlarm(uint16_t code, uint8_t color, int32_t blinkCount, uint16_t blinkDelay);
void emitAlarm(int code);
void cbSubscribe(const GenericCallback *callbacks, size_t callbacksSize);
void runCb(const char *methodName, JsonObject &params);
void onWsEventCb(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
bool ifaceLoopEnable();
void ifaceLoopCb();
void ifaceLoopDisable();
bool wifiOtaEnable();
void wifiOtaLoopCb();
void wifiOtaDisable();
void tbLoopCb();
void tbLoopDisable();
bool tbKeeperEnable();
void tbKeeperCb();
void tbKeeperDisable();
void tbKeeperProvCb();
void updateSpiffsCb();

ESP32SerialLogger serial_logger;
ESP32UDPLogger udp_logger;
LogManager *log_manager = LogManager::GetInstance(LogLevel::VERBOSE);
WiFiClientSecure ssl = WiFiClientSecure();
WiFiMulti wifiMulti;
Config config;
ConfigCoMCU configcomcu;
ThingsBoardSized<DOCSIZE_MIN, 64, LogManager> tb(ssl);
ESP32Time rtc(0);
GCB m_gcb[10];
AsyncWebServer web(80);
AsyncWebSocket ws("/ws");
Scheduler r;

Task wifiKeeperLoop(30 * TASK_SECOND, TASK_FOREVER, &wifiKeeperCb, &r, 0, &wifiKeeperEnable, NULL, 0);
Task tbKeeper(30 * TASK_SECOND, TASK_FOREVER, &tbKeeperCb, &r, 0, &tbKeeperEnable, &tbKeeperDisable, 0);
Task ifaceLoop(10 * TASK_MILLISECOND, TASK_FOREVER, &ifaceLoopCb, &r, 0, &ifaceLoopEnable, &ifaceLoopDisable, 0);
Task tbLoop(10 * TASK_MILLISECOND, TASK_FOREVER, &tbLoopCb, &r, 0, NULL, &tbLoopDisable, 0);
Task wifiOtaLoop(10 * TASK_MILLISECOND, TASK_FOREVER, &wifiOtaLoopCb, &r, 0, &wifiOtaEnable, &wifiOtaDisable, 0);
Task updateSpiffs(TASK_IMMEDIATE, TASK_ONCE, &updateSpiffsCb, &r, 0, NULL, NULL, 0);

void startup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  config.logLev = 1;
  log_manager->add_logger(&serial_logger);
  log_manager->add_logger(&udp_logger);
  log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);
  if(!SPIFFS.begin(true))
  {
    //configReset();
    configLoadFailSafe();
    log_manager->warn(PSTR(__func__), PSTR("Problem with file system. Failsafe config was loaded.\n"));
  }
  else
  {
    log_manager->info(PSTR(__func__), PSTR("Loading config...\n"));
    configLoad();
    log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);
  }
  #ifdef USE_SERIAL2
    log_manager->debug(PSTR(__func__), PSTR("Serial 2 - CoMCU Activated!\n"));
    Serial2.begin(115200, SERIAL_8N1, S2_RX, S2_TX);
  #endif

  log_manager->debug(PSTR(__func__), PSTR("Startup time: %s\n"), rtc.getDateTime().c_str());

  wifiKeeperLoop.enable();
  setAlarm(0, 0, 3, 100);
}


void cbSubscribe(const GCB *callbacks, size_t callbacksSize)
{
  for (size_t i = 0; i < callbacksSize; ++i) {
    m_gcb[i] = callbacks[i];
  }
}

void runCb(const char *methodName, JsonObject &params){
  for (size_t i = 0; i < sizeof(m_gcb) / sizeof(*m_gcb); ++i) {
    if (m_gcb[i].m_cb && !strcmp(m_gcb[i].m_name, methodName)) {
      log_manager->debug(PSTR(__func__), PSTR("calling generic RPC: %s\n"), m_gcb[i].m_name);
      m_gcb[i].m_cb(params);
      break;
    }
  }
}

bool ifaceLoopEnable(){
  if(WiFi.isConnected() && config.fIface){
    web.begin();
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html").setAuthentication(config.htU,config.htP);
    ws.onEvent(onWsEventCb);
    ws.setAuthentication(config.htU, config.htP);
    ws.enable(true);
    web.addHandler(&ws);
    return true;
  }
  return false;
}

void ifaceLoopCb(){
  ws.cleanupClients();
}

void ifaceLoopDisable(){
  web.end();
  ws.closeAll();
  ws.enable(false);
}

bool wifiOtaEnable(){
  if(config.fWOTA){
    ArduinoOTA.setHostname(config.hname);
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
            SPIFFS.end();
        log_manager->warn(PSTR(__func__),PSTR("Starting OTA %s\n"), type.c_str());
      })
      .onEnd([]()
      {
        log_manager->warn(PSTR(__func__),PSTR("Device rebooting...\n"));
        reboot();
      })
      .onProgress([](unsigned int progress, unsigned int total)
      {
        log_manager->warn(PSTR(__func__),PSTR("OTA progress: %d/%d\n"), progress, total);
      })
      .onError([](ota_error_t error)
      {
        log_manager->warn(PSTR(__func__),PSTR("OTA Failed: %d\n"), error);
        reboot();
      }
    );
    return true;
  }
  return false;
}

void wifiOtaLoopCb(){
  ArduinoOTA.handle();
}

void wifiOtaDisable(){
  ArduinoOTA.end();
}

void tbLoopCb(){
  tb.loop();
}
void tbLoopDisable(){
  tb.disconnect();
}

bool tbKeeperEnable(){
  if(!config.fIoT){
    return false;
  }

  int freeHeap = ESP.getFreeHeap();
  log_manager->info(PSTR(__func__),PSTR("Initializing IoT, available memory: %d\n"), freeHeap);
  if(freeHeap < 92000)
  {
    log_manager->warn(PSTR(__func__),PSTR("Unable to init IoT, insufficient memory: %d\n"), freeHeap);
    return false;
  }

  if(!WiFi.isConnected()){
    log_manager->warn(PSTR(__func__),PSTR("Unable to init IoT, WiFi is not connected!\n"));
    return false;
  }

  tb.setBufferSize(DOCSIZE_MIN);

  tbLoop.setInterval(25 * TASK_MILLISECOND);
  tbLoop.setIterations(TASK_FOREVER);
  tbLoop.enable();
  return true;
}
void tbKeeperCb(){
  if(!config.provSent)
  {
    tbKeeper.setCallback(tbKeeperProvCb);
    tbKeeper.forceNextIteration();
  }
  else{
    if(!tb.connected())
    {
      log_manager->info(PSTR(__func__),PSTR("Connecting to broker %s:%d\n"), config.broker, config.port);
      if(!tb.connect(config.broker, config.accTkn, config.port, config.name))
      {
        config.tbDisco++;
        log_manager->warn(PSTR(__func__),PSTR("Failed to connect to IoT Broker %s (%d)\n"), config.broker, config.tbDisco);
        StaticJsonDocument<1> doc;
        JsonObject params = doc.template as<JsonObject>();
        runCb("onTbDisconnected", params);
        if(config.tbDisco >= 10){
          config.provSent = 0;
          config.tbDisco = 0;
        }
        return;
      }
      StaticJsonDocument<1> doc;
      JsonObject params = doc.template as<JsonObject>();
      runCb("onTbConnected", params);
      setAlarm(0, 0, 3, 100);
      log_manager->info(PSTR(__func__),PSTR("IoT Connected!\n"));
    }
  }
}

void tbKeeperProvCb(){
  log_manager->info(PSTR(__func__),PSTR("Starting provision initiation to %s:%d\n"),  config.broker, config.port);
  if(tb.connect(config.broker, "provision", config.port))
  {
    log_manager->info(PSTR(__func__),PSTR("Connected to provisioning server: %s:%d\n"),  config.broker, config.port);

    GenericCallback cb[2] = {
      { "provisionResponse", processProvisionResponse },
      { "provisionResponse", processProvisionResponse }
    };
    if(tb.callbackSubscribe(cb, 2))
    {
      if(tb.sendProvisionRequest(config.name, config.provDK, config.provDS))
      {
        log_manager->info(PSTR(__func__),PSTR("Provision request was sent! Waiting for response.\n"));
      }
    }
  }
  else
  {
    log_manager->warn(PSTR(__func__),PSTR("Failed to connect to provisioning server: %s:%d\n"),  config.broker, config.port);
    return;
  }
}

void tbKeeperDisable(){
  tbLoop.disable();
}

void emitAlarm(int code){
  StaticJsonDocument<DOCSIZE_MIN> doc;
  doc["alarm"] = code;
  tb.sendTelemetryDoc(doc);
  log_manager->error(PSTR(__func__), PSTR("%i\n"), code);
  JsonObject params = doc.template as<JsonObject>();
  runCb("emitAlarmWs", params);
}

void rtcUpdate(long ts){
  if(ts == 0){
    configTime(config.gmtOff, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)){
      rtc.setTimeStruct(timeinfo);
      log_manager->debug(PSTR(__func__), PSTR("Updated time via NTP: %s GMT Offset:%d (%d) \n"), rtc.getDateTime().c_str(), config.gmtOff, config.gmtOff / 3600);
    }
  }else{
      rtc.setTime(ts);
      log_manager->debug(PSTR(__func__), PSTR("Updated time via timestamp: %s\n"), rtc.getDateTime().c_str());
  }
}

void cbWifiOnConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->info(PSTR(__func__),PSTR("WiFi Connected to %s\n"), WiFi.SSID().c_str());
}

void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->warn(PSTR(__func__),PSTR("WiFi (%s and %s) Disconnected!\n"), config.wssid, config.dssid);
  if(config.fIoT){
    tbKeeper.disable();
  }
  if(config.fIface){
    ifaceLoop.disable();
  }
  if(config.fWOTA){
    wifiOtaLoop.disable();
  }
}

void cbWiFiOnLostIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->warn(PSTR(__func__),PSTR("WiFi (%s) IP Lost!\n"), WiFi.SSID().c_str());
}

void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  IPAddress ip = WiFi.localIP();
  char ipa[25];
  sprintf(ipa, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  log_manager->warn(PSTR(__func__),PSTR("WiFi (%s) IP Assigned: %s!\n"), WiFi.SSID().c_str(), ipa);

  ssl.setCACert(CA_CERT);

  MDNS.begin(config.hname);
  MDNS.addService("http", "tcp", 80);
  log_manager->info(PSTR(__func__),PSTR("Started MDNS on %s\n"), config.hname);

  rtcUpdate(0);
  if(config.fWOTA){
    wifiOtaLoop.enable();
  }

  if(config.fIface){
    ifaceLoop.enable();
  }

  if(config.fIoT){
    tbKeeper.enable();
  }

  setAlarm(0, 0, 3, 100);
}

bool wifiKeeperEnable()
{
  log_manager->debug(PSTR(__func__),PSTR("Initializing wifi network...\n"));
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hname);
  WiFi.setAutoReconnect(true);
  if(!config.wssid || *config.wssid == 0x00 || strlen(config.wssid) > 32)
  {
    configLoadFailSafe();
    log_manager->warn(PSTR(__func__), PSTR("SSID too long or missing! Failsafe config was loaded.\n"));
  }
  wifiMulti.addAP(config.wssid, config.wpass);
  wifiMulti.addAP(config.dssid, config.dpass);

  WiFi.onEvent(cbWifiOnConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(cbWiFiOnDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(cbWiFiOnLostIp, ARDUINO_EVENT_WIFI_STA_LOST_IP);
  WiFi.onEvent(cbWiFiOnGotIp, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  return true;
}

void wifiKeeperCb(){
  wifiMulti.run();
}

void onWsEventCb(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  StaticJsonDocument<DOCSIZE_MIN> doc;
  DeserializationError err = deserializeJson(doc, data);

  if(type == WS_EVT_CONNECT){
    log_manager->debug(PSTR(__func__), PSTR("ws[%s][%u] connect\n"), server->url(), client->id());
    doc["evType"] = (int)WS_EVT_CONNECT;
    JsonObject root = doc.template as<JsonObject>();
    runCb("wsEvent", root);
  } else if(type == WS_EVT_DISCONNECT){
    log_manager->debug(PSTR(__func__), PSTR("ws[%s][%u] disconnect\n"), server->url(), client->id());
    doc["evType"] = (int)WS_EVT_DISCONNECT;
    JsonObject root = doc.template as<JsonObject>();
    runCb("wsEvent", root);
  } else if(type == WS_EVT_ERROR){
    log_manager->warn(PSTR(__func__), PSTR("ws[%s][%u] error(%u): %s\n"), server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    doc["evType"] = (int)WS_EVT_ERROR;
    JsonObject root = doc.template as<JsonObject>();
    runCb("wsEvent", root);
  } else if(type == WS_EVT_PONG){
    doc["evType"] = (int)WS_EVT_PONG;
    JsonObject root = doc.template as<JsonObject>();
    log_manager->debug(PSTR(__func__), PSTR("ws[%s][%u] pong[%u]: %s\n"), server->url(), client->id(), len, (len)?(char*)data:"");
    runCb("wsEvent", root);
  } else if(type == (int)WS_EVT_DATA){
    if (err == DeserializationError::Ok)
    {
      doc["evType"] = (int)WS_EVT_DATA;
      JsonObject root = doc.template as<JsonObject>();
      log_manager->debug(PSTR(__func__), PSTR("WS message parsing %s\n"), err.c_str());
      runCb("wsEvent", root);
    }
    else
    {
      log_manager->warn(PSTR(__func__), PSTR("WS message parsing error: %s\n"), err.c_str());
    }
  }
}

void udawa() {
  r.execute();
}

void updateSpiffsCb(){
  esp32FOTA esp32FOTA(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION);

  CryptoMemAsset  *MyRootCA = new CryptoMemAsset("Root CA", CA_CERT, strlen(CA_CERT)+1 );

  esp32FOTA.setManifestURL(mnfstUrl);
  esp32FOTA.setRootCA(MyRootCA);
  esp32FOTA.setCertFileSystem(nullptr);
  esp32FOTA.setProgressCb( [](size_t progress, size_t size) {
      if( progress == size || progress == 0 ) Serial.println();
      Serial.print(".");
  });
  esp32FOTA.setUpdateCheckFailCb( [](int partition, int error_code) {
    Serial.printf("Update could validate %s partition (error %d)\n", partition==U_SPIFFS ? "spiffs" : "firmware", error_code );
    // error codes:
    //  -1 : partition not found
    //  -2 : validation (signature check) failed
  });
  esp32FOTA.setUpdateBeginFailCb( [](int partition) {

  });
  esp32FOTA.setUpdateEndCb( [](int partition) {

  });
  esp32FOTA.setUpdateFinishedCb( [](int partition, bool restart_after) {
    StaticJsonDocument<1> doc;
    doc["partition"] = partition;
    JsonObject root = doc.template as<JsonObject>();
    configSave();
    configCoMCUSave();
    runCb("onUpdateFinished", root);
  });

  esp32FOTA.execHTTPcheck();
  esp32FOTA.execOTASPIFFS();
}

void setBuzzer(int32_t beepCount, uint16_t beepDelay){
  StaticJsonDocument<DOCSIZE_MIN> doc;
  JsonObject params = doc.createNestedObject("params");
  doc["method"] = "sBuz";
  params["beepCount"] = beepCount;
  params["beepDelay"] = beepDelay;
  serialWriteToCoMcu(doc, 0);
}

void setLed(uint8_t r, uint8_t g, uint8_t b, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay){
  StaticJsonDocument<DOCSIZE_MIN> doc;
  JsonObject params = doc.createNestedObject("params");
  params["r"] = r;
  params["g"] = g;
  params["b"] = b;
  params["isBlink"] = isBlink;
  params["blinkCount"] = blinkCount;
  params["blinkDelay"] = blinkDelay;
  serialWriteToCoMcu(doc, 0);
}

void setLed(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay){
uint8_t r, g, b;
  switch (color)
  {
  //Auto by network
  case 0:
    if(tb.connected()){
      r = configcomcu.lON == 0 ? 255 : 0;
      g = configcomcu.lON == 0 ? 255 : 0;
      b = configcomcu.lON;
    }
    else if(WiFi.status() == WL_CONNECTED){
      r = configcomcu.lON == 0 ? 255 : 0;
      g = configcomcu.lON;
      b = configcomcu.lON == 0 ? 255 : 0;
    }
    else{
      r = configcomcu.lON;
      g = configcomcu.lON == 0 ? 255 : 0;
      b = configcomcu.lON == 0 ? 255 : 0;
    }
    break;
  //RED
  case 1:
    r = configcomcu.lON;
    g = configcomcu.lON == 0 ? 255 : 0;
    b = configcomcu.lON == 0 ? 255 : 0;
    break;
  //GREEN
  case 2:
    r = configcomcu.lON == 0 ? 255 : 0;
    g = configcomcu.lON;
    b = configcomcu.lON == 0 ? 255 : 0;
    break;
  //BLUE
  case 3:
    r = configcomcu.lON == 0 ? 255 : 0;
    g = configcomcu.lON == 0 ? 255 : 0;
    b = configcomcu.lON;
    break;
  default:
    r = configcomcu.lON;
    g = configcomcu.lON;
    b = configcomcu.lON;
  }
  StaticJsonDocument<DOCSIZE_MIN> doc;
  doc["method"] = "sLed";
  JsonObject params = doc.createNestedObject("params");
  params["r"] = r;
  params["g"] = g;
  params["b"] = b;
  params["isBlink"] = isBlink;
  params["blinkCount"] = blinkCount;
  params["blinkDelay"] = blinkDelay;
  serialWriteToCoMcu(doc, 0);
}

void setAlarm(uint16_t code, uint8_t color, int32_t blinkCount, uint16_t blinkDelay){
  setLed(color, 1, blinkCount, blinkDelay);
  setBuzzer(blinkCount, blinkDelay);
  if(code > 0){
    emitAlarm(code);
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

void configReset()
{
  /*bool formatted = SPIFFS.format();
  if(formatted)
  {
    log_manager->info(PSTR(__func__),PSTR("SPIFFS formatting success.\n"));
  }
  else
  {
    log_manager->warn(PSTR(__func__),PSTR("SPIFFS formatting failed.\n"));
  }*/
  File file;
  file = SPIFFS.open(configFile, FILE_WRITE);
  if (!file) {
    log_manager->warn(PSTR(__func__),PSTR("Failed to create config file. Config reset is cancelled.\n"));
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
  doc["accTkn"] = accTkn;
  doc["provSent"] = false;
  doc["provDK"] = provDK;
  doc["provDS"] = provDS;
  doc["logLev"] = 5;
  doc["gmtOff"] = 28880;

  doc["fIoT"] = 1;
  doc["fWOTA"] = 1;
  doc["fIface"] = 1;

  doc["hname"] = "UDAWA" + String(dv);

  doc["htU"] = "UDAWA";
  doc["htP"] = "defaultkey";

  doc["logIP"] = "192.168.18.255";
  doc["logPrt"] = 29514;

  size_t size = serializeJson(doc, file);
  file.close();

  log_manager->info(PSTR(__func__),PSTR("Resetted config file (size: %d) is written successfully...\n"), size);
  file = SPIFFS.open(configFile, FILE_READ);
  if (!file)
  {
    log_manager->warn(PSTR(__func__),PSTR("Failed to open the config file!"));
  }
  else
  {
    log_manager->info(PSTR(__func__),PSTR("New config file opened, size: %d\n"), file.size());

    if(file.size() < 1)
    {
      log_manager->warn(PSTR(__func__),PSTR("Config file size is abnormal: %d, trying to rewrite...\n"), file.size());

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
  strlcpy(config.accTkn, accTkn, sizeof(config.accTkn));
  config.provSent = false;
  config.port = port;
  strlcpy(config.provDK, provDK, sizeof(config.provDK));
  strlcpy(config.provDS, provDS, sizeof(config.provDS));
  config.logLev = 5;
  config.gmtOff = 28800;

  config.fIoT = 1;
  config.fWOTA = 1;
  config.fIface = 1;
  String hname = "UDAWA" + String(dv);
  strlcpy(config.hname, hname.c_str(), sizeof(config.hname));
  strlcpy(config.htU, "UDAWA", sizeof(config.htU));
  strlcpy(config.htP, "defaultkey", sizeof(config.htP));
  strlcpy(config.logIP, "192.168.18.255", sizeof(config.logIP));
  config.logPrt = 29514;
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
    log_manager->warn(PSTR(__func__),PSTR("Config file size is abnormal: %d. Closing file and trying to reset...\n"), file.size());
    configReset();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;
  DeserializationError error = deserializeJson(doc, file);

  if(error)
  {
    log_manager->warn(PSTR(__func__),PSTR("Failed to load config file! (%s - %s - %d). Falling back to failsafe.\n"), configFile, error.c_str(), file.size());
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
    if(doc["model"] != nullptr){strlcpy(config.model, doc["model"].as<const char*>(), sizeof(config.model));}
    if(doc["group"] != nullptr){strlcpy(config.group, doc["group"].as<const char*>(), sizeof(config.group));}
    if(doc["broker"] != nullptr){strlcpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker));}
    if(doc["port"] != nullptr){config.port = doc["port"].as<uint16_t>();}
    if(doc["wssid"] != nullptr){strlcpy(config.wssid, doc["wssid"].as<const char*>(), sizeof(config.wssid));}
    if(doc["wpass"] != nullptr){strlcpy(config.wpass, doc["wpass"].as<const char*>(), sizeof(config.wpass));}
    if(doc["dssid"] != nullptr){strlcpy(config.dssid, doc["dssid"].as<const char*>(), sizeof(config.dssid));}
    if(doc["dpass"] != nullptr){strlcpy(config.dpass, doc["dpass"].as<const char*>(), sizeof(config.dpass));}
    if(doc["upass"] != nullptr){strlcpy(config.upass, doc["upass"].as<const char*>(), sizeof(config.upass));}
    if(doc["accTkn"] != nullptr){strlcpy(config.accTkn, doc["accTkn"].as<const char*>(), sizeof(config.accTkn));}
    if(doc["provDK"] != nullptr){strlcpy(config.provDK, doc["provDK"].as<const char*>(), sizeof(config.provDK));}
    if(doc["provDS"] != nullptr){strlcpy(config.provDS, doc["provDS"].as<const char*>(), sizeof(config.provDS));}
    if(doc["provSent"] != nullptr){config.provSent = doc["provSent"].as<bool>();}
    if(doc["logLev"] != nullptr){config.logLev = doc["logLev"].as<uint8_t>(); log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);;}
    if(doc["gmtOff"] != nullptr){config.gmtOff = doc["gmtOff"].as<int>();}
    if(doc["fIoT"] != nullptr){config.fIoT = doc["fIoT"].as<int>();}
    if(doc["htU"] != nullptr){strlcpy(config.htU, doc["htU"].as<const char*>(), sizeof(config.htU));}
    if(doc["htP"] != nullptr){strlcpy(config.htP, doc["htP"].as<const char*>(), sizeof(config.htP));}
    if(doc["fWOTA"] != nullptr){config.fWOTA = doc["fWOTA"].as<bool>();}
    if(doc["fIface"] != nullptr){config.fIface = doc["fIface"].as<bool>();}
    if(doc["hname"] != nullptr){strlcpy(config.hname, doc["hname"].as<const char*>(), sizeof(config.hname));}
    if(doc["logIP"] != nullptr){strlcpy(config.logIP, doc["logIP"].as<const char*>(), sizeof(config.logIP));}
    if(doc["logPrt"] != nullptr){config.logPrt = doc["logPrt"].as<uint16_t>();}

    log_manager->info(PSTR(__func__),PSTR("Config loaded successfuly.\n"));
  }
  file.close();
}

void configSave()
{
  if(!SPIFFS.remove(configFile))
  {
    log_manager->warn(PSTR(__func__),PSTR("Failed to delete the old configFile: %s\n"), configFile);
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
  doc["accTkn"] = config.accTkn;
  doc["provSent"] = config.provSent;
  doc["provDK"] = config.provDK;
  doc["provDS"] = config.provDS;
  doc["logLev"] = config.logLev;
  doc["gmtOff"] = config.gmtOff;

  doc["fIoT"] = config.fIoT;
  doc["fWOTA"] = config.fWOTA;
  doc["fIface"] = config.fIface;
  doc["hname"] = config.hname;
  doc["htU"] = config.htU;
  doc["htP"] = config.htP;

  doc["logIP"] = config.logIP;
  doc["logPrt"] = config.logPrt;

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

  doc["fP"] = false;

  doc["bFr"] = 1600;
  doc["fB"] = 1;

  doc["pBz"] = 2;
  doc["pLR"] = 3;
  doc["pLG"] = 5;
  doc["pLB"] = 6;
  doc["lON"] = 0;

  serializeJson(doc, file);
  file.close();

  log_manager->info(PSTR(__func__),PSTR("ConfigCoMCU hard reset!"));
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

    if(doc["fP"] != nullptr){configcomcu.fP = doc["fP"].as<bool>();}
    if(doc["bFr"] != nullptr){configcomcu.bFr = doc["bFr"].as<uint16_t>();}
    if(doc["fB"] != nullptr){configcomcu.fB = doc["fB"].as<bool>();}
    if(doc["pBz"] != nullptr){configcomcu.pBz = doc["pBz"].as<uint8_t>();}
    if(doc["pLR"] != nullptr){configcomcu.pLR = doc["pLR"].as<uint8_t>();}
    if(doc["pLG"] != nullptr){configcomcu.pLG = doc["pLG"].as<uint8_t>();}
    if(doc["pLB"] != nullptr){configcomcu.pLB = doc["pLB"].as<uint8_t>();}
    if(doc["lON"] != nullptr){configcomcu.lON = doc["lON"].as<uint8_t>();}

    log_manager->info(PSTR(__func__),PSTR("ConfigCoMCU loaded successfuly.\n"));
  }
  file.close();
}

void configCoMCUSave()
{
  if(!SPIFFS.remove(configFileCoMCU))
  {
    log_manager->warn(PSTR(__func__),PSTR("Failed to delete the old configFileCoMCU: %s\n"), configFileCoMCU);
  }
  File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
  if (!file)
  {
    file.close();
    return;
  }

  StaticJsonDocument<DOCSIZE> doc;

  doc["fP"] = configcomcu.fP;

  doc["bFr"] = configcomcu.bFr;
  doc["fB"] = configcomcu.fB;

  doc["pBz"] = configcomcu.pBz;
  doc["pLR"] = configcomcu.pLR;
  doc["pLG"] = configcomcu.pLG;
  doc["pLB"] = configcomcu.pLB;
  doc["lON"] = configcomcu.lON;

  serializeJson(doc, file);
  file.close();
}

void syncConfigCoMCU()
{
  configCoMCULoad();

  StaticJsonDocument<DOCSIZE_MIN> doc;
  doc["fP"] = configcomcu.fP;
  doc["bFr"] = configcomcu.bFr;
  doc["fB"] = configcomcu.fB;
  doc["pBz"] = configcomcu.pBz;
  doc["method"] = "sCfg";
  serialWriteToCoMcu(doc, 0);
  doc.clear();
  doc["pLR"] = configcomcu.pLR;
  doc["pLG"] = configcomcu.pLG;
  doc["pLB"] = configcomcu.pLB;
  doc["lON"] = configcomcu.lON;
  doc["method"] = "sCfg";
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
  tbKeeper.setCallback(tbKeeperCb);
  log_manager->info(PSTR(__func__),PSTR("Received device provision response\n"));
  int jsonSize = measureJson(data) + 1;
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);

  if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0)
  {
    log_manager->warn(PSTR(__func__),PSTR("Provision response contains the error: %s\n"), data["errorMsg"].as<const char*>());
    return callbackResponse("provisionResponse", 1);
  }
  else
  {
    log_manager->info(PSTR(__func__),PSTR("Provision response credential type: %s\n"), data["credentialsType"].as<const char*>());
  }
  if (strncmp(data["credentialsType"], "ACCESS_TOKEN", strlen("ACCESS_TOKEN")) == 0)
  {
    log_manager->info(PSTR(__func__),PSTR("ACCESS TOKEN received: %s\n"), data["credentialsValue"].as<String>().c_str());
    strlcpy(config.accTkn, data["credentialsValue"].as<String>().c_str(), sizeof(config.accTkn));
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
  reboot();
  return callbackResponse("provisionResponse", 1);
}

void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, bool isRpc)
{
  serializeJson(doc, Serial2);
  StringPrint stream;
  serializeJson(doc, stream);
  String result = stream.str();
  log_manager->debug(PSTR(__func__),PSTR("Sent to CoMCU: %s\n"), result.c_str());
  if(isRpc)
  {
    delay(50);
    doc.clear();
    serialReadFromCoMcu(doc);
  }
}

void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc)
{
  StringPrint stream;
  String result;
  ReadLoggingStream loggingStream(Serial2, stream);
  DeserializationError err = deserializeJson(doc, loggingStream);
  result = stream.str();
  if (err == DeserializationError::Ok)
  {
    log_manager->debug(PSTR(__func__),PSTR("Received from CoMCU: %s\n"), result.c_str());
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

void setCoMCUPin(uint8_t pin, uint8_t op, uint8_t mode, uint16_t aval, uint8_t state)
{
  StaticJsonDocument<DOCSIZE_MIN> doc;
  JsonObject params = doc.createNestedObject("params");
  doc["method"] = "sPin";
  params["pin"] = pin;
  params["mode"] = mode;
  params["op"] = op;
  params["state"] = state;
  params["aval"] = aval;
  serialWriteToCoMcu(doc, false);
}

double round2(double value) {
   return (int)(value * 100 + 0.5) / 100.0;
}

const uint32_t MAX_INT = 0xFFFFFFFF;
uint32_t micro2milli(uint32_t hi, uint32_t lo)
{
  if (hi >= 1000)
  {
    log_manager->warn(PSTR(__func__), PSTR("Cannot store milliseconds in uint32!\n"));
  }

  uint32_t r = (lo >> 16) + (hi << 16);
  uint32_t ans = r / 1000;
  r = ((r % 1000) << 16) + (lo & 0xFFFF);
  ans = (ans << 16) + r / 1000;

  return ans;
}


static SemaphoreHandle_t udpSemaphore = NULL;
void ESP32UDPLogger::log_message(const char *tag, LogLevel level, const char *fmt, va_list args)
{
    if((int)level > config.logLev || !WiFi.isConnected()){
      return;
    }
    if(udpSemaphore == NULL)
    {
        udpSemaphore = xSemaphoreCreateMutex();
    }

    if(udpSemaphore != NULL && xSemaphoreTake(udpSemaphore, (TickType_t) 20))
    {
        int size = 512;
        WiFiUDP udp;
        char data[size];
        vsnprintf(data, size, fmt, args);
        String msg = String(get_error_char(level)) + "~[" + config.name + "] " + String(tag) + "~" + String(data);
        
        udp.beginPacket(config.logIP, config.logPrt);
        udp.write((uint8_t*)msg.c_str(), msg.length());
        udp.endPacket();

        xSemaphoreGive(udpSemaphore);
    }
    else{
        printf("Could not get UDP semaphore.\n");
    }
}

} // namespace libudawa
#endif