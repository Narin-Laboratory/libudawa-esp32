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
#ifdef USE_WIFI_OTA
#include <ArduinoOTA.h>
#endif
#include "logging.h"
#include "serialLogger.h"
#include <ESP32Time.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <Update.h>
#include <Espressif_Updater.h>
#ifdef USE_WEB_IFACE
#include <Crypto.h>
#include <SHA256.h>
#include "mbedtls/md.h"
#include <map>
#ifdef USE_ASYNC_WEB
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif
#ifndef USE_ASYNC_WEB
#include <WebServer.h>
#include <WebSocketsServer.h>
#endif
#endif
#ifdef USE_SDCARD_LOG
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#endif
#ifdef USE_HW_RTC
#include <ErriezDS3231.h>
#endif
#include "BinDownloader.h"
#define countof(a) (sizeof(a) / sizeof(a[0]))
#define COMPILED __DATE__ " " __TIME__
#define S2_RX 16
#define S2_TX 17
#ifndef STACKSIZE_WIFIKEEPER 
  #define STACKSIZE_WIFIKEEPER 4096
#endif
#ifndef STACKSIZE_SETALARM 
#define STACKSIZE_SETALARM 4096
#endif
#ifndef STACKSIZE_WIFIOTA 
#define STACKSIZE_WIFIOTA 4096
#endif
#ifndef STACKSIZE_TB 
#define STACKSIZE_TB 4096
#endif
#ifndef STACKSIZE_IFACE 
#define STACKSIZE_IFACE 4096
#endif
#ifndef DOCSIZE
  #define DOCSIZE 1024
#endif
#ifndef DOCSIZE_MIN
  #define DOCSIZE_MIN 384
#endif
#ifndef DOCSIZE_SETTINGS
  #define DOCSIZE_SETTINGS 2048
#endif

namespace libudawa
{

const char* configFile = "/cfg.json";
const char* configFileCoMCU = "/comcu.json";

struct Config
{
  unsigned long ECP;
  int CC;
  bool SM;

  char hwid[16];
  char name[24];
  char model[16];
  char group[16];
  uint8_t logLev;

  char broker[48];
  uint16_t port;
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
  bool fIface;
  char hname[40];
  char htU[24];
  char htP[24];
  char webApiKey[32];

  char logIP[16] = "255.255.255.255";
  uint16_t logPrt = 29514;

  #ifdef USE_WEB_IFACE
  uint8_t wsCount = 0;
  unsigned long rateLimitInterval = 1000; // rate limit interval in milliseconds
  unsigned long blockInterval = 60000; // block interval in milliseconds

  #endif

  #ifdef USE_SDCARD_LOG
  uint64_t cardSize = 0;
  uint64_t cardByte = 0;
  uint64_t cardUsed = 0;
  #endif
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

#ifdef USE_WIFI_LOGGER
class ESP32UDPLogger : public ILogHandler
{
    public:
        void log_message(const char *tag, const LogLevel level, const char *fmt, va_list args) override;
};
#endif

#ifdef USE_WEB_IFACE
std::map<uint32_t, bool> clientAuthenticationStatus;
std::map<IPAddress, unsigned long> clientAuthAttemptTimestamps; 
#endif

uint32_t micro2milli(uint32_t hi, uint32_t lo);
double round2(double value);
void udawa();
void reboot(int countdown);
char* getDeviceId();
void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info);
void configLoadFailSafe();
void configLoad();
void configSave();
void configReset();
void configCoMCULoadFailSafe();
void configCoMCULoad();
void configCoMCUSave();
void configCoMCUReset();
void (*onSaveSettings)();
void (*onSaveStates)();
bool loadFile(const char* filePath, char* buffer);
void processProvisionResponse(const Provision_Data &data);
void processSharedAttributeRequest(const Shared_Attribute_Data &data);
void processClientAttributeRequest(const Shared_Attribute_Data &data);
void processSharedAttributeUpdate(const Shared_Attribute_Data &data);
void (*processSharedAttributeUpdateCb)(const Shared_Attribute_Data &data);
void startup();
void wifiKeeperTR(void *arg);
void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, bool isRpc, int wait = 50);
void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, int wait = 50);
void syncConfigCoMCU();
void readSettings(StaticJsonDocument<DOCSIZE_SETTINGS> &doc,const char* path);
void writeSettings(StaticJsonDocument<DOCSIZE_SETTINGS> &doc, const char* path);
void setCoMCUPin(uint8_t pin, uint8_t op, uint8_t mode, uint16_t aval, uint8_t state);
void rtcUpdate(long ts = 0);
void setBuzzer(int32_t beepCount, uint16_t beepDelay);
void setLed(uint8_t r, uint8_t g, uint8_t b, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay);
void setLed(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay);
void setAlarm(uint16_t code, uint8_t color, int32_t blinkCount, uint16_t blinkDelay);
void setAlarmTR(void *arg);
void emitAlarm(int code);
void (*emitAlarmCb)(const int code);
#ifdef USE_WEB_IFACE
#ifdef USE_ASYNC_WEB
void hashApiKeyWithSalt(const char *apiKey, const char *salt, char *hashResultHex);
void onWsEventCb(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
#endif
#ifndef USE_ASYNC_WEB
void onWsEventCb(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void webSendFile(String path, String type);
#endif
void (*wsEventCb)(const JsonObject &payload);
bool wsSendTXT(uint32_t id, const char *buffer);
bool wsBroadcastTXT(const char *buffer);
void ifaceTR(void *arg);
#endif
#ifdef USE_WIFI_OTA
void wifiOtaTR(void *arg);
#endif
void TBTR(void *arg);
void tbOtaFinishedCb(const bool& success);
void tbOtaProgressCb(const uint32_t& currentChunk, const uint32_t& totalChuncks);
void (*httpOtaOnUpdateFinishedCb)(const int partition);
void updateSpiffs();
void (*onTbDisconnectedCb)();
void (*onTbConnectedCb)();
RPC_Response processConfigSave(const RPC_Data &data);
RPC_Response processConfigCoMCUSave(const RPC_Data &data);
RPC_Response processSaveSettings(const RPC_Data &data);
RPC_Response processSetPanic(const RPC_Data &data);
void (*processSetPanicCb)(const RPC_Data &data);
RPC_Response processUpdateSpiffs(const RPC_Data &data);
RPC_Response processReboot(const RPC_Data &data);
RPC_Response processGenericClientRPC(const RPC_Data &data);
RPC_Response (*processGenericClientRPCCb)(const RPC_Data &data);
RPC_Response processUpdateApp(const RPC_Data &data);
void processFwCheckAttributeRequest(const Shared_Attribute_Data &data);
void syncClientAttr(uint8_t direction);
void (*onSyncClientAttrCb)(uint8_t);
bool tbSendAttribute(const char *buffer);
bool tbSendTelemetry(const char *buffer);
void (*tbloggerCb)(const char *error);
void onTbLogger(const char *error);
void (*onMQTTUpdateStartCb)();
void (*onMQTTUpdateEndCb)();
#ifdef USE_DISK_LOG
void setupCardLogger();
void writeCardLogger(StaticJsonDocument<DOCSIZE_MIN> &doc);
void wsStreamCardLogger(uint32_t id, String fileName);
void deleteLogFile(String fileName);
#endif

class TBLogger {
  public:
    static void log(const char *error) {
      tbloggerCb(error);
    }
};

#ifdef USE_SDCARD_LOG
#define SD_MISO     19
#define SD_MOSI     23
#define SD_SCLK     18
#define SD_CS       5
SPIClass sdSPI(VSPI);
#endif

ESP32SerialLogger serial_logger;
#ifdef USE_WIFI_LOGGER
ESP32UDPLogger udp_logger;
WiFiUDP udp;
#endif
LogManager *log_manager = LogManager::GetInstance(LogLevel::VERBOSE);
WiFiClientSecure ssl = WiFiClientSecure();
WiFiMulti wifiMulti;
Config config;
ConfigCoMCU configcomcu;
Espressif_Updater updater;
Arduino_MQTT_Client mqttClient(ssl);
ThingsBoardSized<32, TBLogger> tb(mqttClient, DOCSIZE_MIN);
ESP32Time rtc(0);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
#ifdef USE_HW_RTC
ErriezDS3231 rtcHw;
#endif
#ifdef USE_WEB_IFACE
#ifdef USE_ASYNC_WEB
AsyncWebServer web(80);
AsyncWebSocket ws("/ws");
#endif
#ifndef USE_ASYNC_WEB
WebServer web(80);
WebSocketsServer ws = WebSocketsServer(81);
#endif
#endif
unsigned long LAST_TB_CONNECTED = 0;
bool FLAG_SAVE_SETTINGS = false;
bool FLAG_SAVE_CONFIG = false;
bool FLAG_SAVE_CONFIGCOMCU = false;
bool FLAG_SYNC_CONFIGCOMCU = false;
bool FLAG_SAVE_STATES = false;
bool FLAG_SYNC_CLIENT_ATTR_0 = false;
bool FLAG_SYNC_CLIENT_ATTR_1 = false;
bool FLAG_SYNC_CLIENT_ATTR_2 = false;
bool FLAG_TB_OTA_ACTIVATED = false;
bool FLAG_UPDATE_SPIFFS = false;
bool FLAG_ECP_UPDATED = false;
bool FLAG_SM_CLEARED = false;
bool FLAG_WS_STREAM_SDCARD = false;
bool FLAG_REBOOT_COUNTDOWN = false;
uint32_t GLOBAL_TARGET_CLIENT_ID = 0;
String GLOBAL_LOG_FILE_NAME = "";
unsigned long TIMER_FLAG_REBOOT_COUNTDOWN;
int REBOOT_COUNTDOWN = 10;

// Client-side RPC that can be executed from cloud
const std::array<RPC_Callback, 8U> clientRPCCallbacks = {
  RPC_Callback{ PSTR("configSave"),    processConfigSave },
  RPC_Callback{ PSTR("configCoMCUSave"), processConfigCoMCUSave },
  RPC_Callback{ PSTR("saveSettings"), processSaveSettings },
  RPC_Callback{ PSTR("updateSpiffs"), processUpdateSpiffs },
  RPC_Callback{ PSTR("setPanic"), processSetPanic }, 
  RPC_Callback{ PSTR("reboot"),  processReboot},
  RPC_Callback{ PSTR("updateApp"), processUpdateApp },
  RPC_Callback{ PSTR("generic"), processGenericClientRPC }
};

// Shared attributes we want to request from the server
constexpr std::array<const char*, 1U> REQUESTED_FW_CHECK_SHARED_ATTRIBUTES = {
  FW_VER_KEY
};
const Attribute_Request_Callback fwCheckCb(&processFwCheckAttributeRequest, REQUESTED_FW_CHECK_SHARED_ATTRIBUTES.cbegin(), REQUESTED_FW_CHECK_SHARED_ATTRIBUTES.cend());

const OTA_Update_Callback tbOtaCb(&tbOtaProgressCb, &tbOtaFinishedCb, CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION, &updater, 40, 4096);
const Shared_Attribute_Callback tbSharedAttrUpdateCb(&processSharedAttributeUpdate);

BaseType_t xReturnedWifiKeeper;
BaseType_t xReturnedAlarm;
#ifdef USE_WIFI_OTA
BaseType_t xReturnedWifiOta;
#endif
BaseType_t xReturnedTB;
BaseType_t xReturnedIface;

TaskHandle_t xHandleWifiKeeper = NULL;
TaskHandle_t xHandleAlarm = NULL;
#ifdef USE_WIFI_OTA
TaskHandle_t xHandleWifiOta;
#endif
TaskHandle_t xHandleTB;
#ifdef USE_WEB_IFACE
TaskHandle_t xHandleIface;
#endif

SemaphoreHandle_t xSemaphoreSerialCoMCUWrite = NULL;
SemaphoreHandle_t xSemaphoreSerialCoMCURead = NULL;
SemaphoreHandle_t xSemaphoreUDPLogger = NULL;
SemaphoreHandle_t xSemaphoreSettings = NULL;
SemaphoreHandle_t xSemaphoreConfig = NULL;
SemaphoreHandle_t xSemaphoreConfigCoMCU = NULL;
SemaphoreHandle_t xSemaphoreTBSend = NULL;
SemaphoreHandle_t xSemaphoreWSSend = NULL;
SemaphoreHandle_t xSemaphoreCardLogger = NULL;

struct AlarmMessage
{
    uint16_t code;
    uint8_t color; 
    int32_t blinkCount; 
    uint16_t blinkDelay;
};
QueueHandle_t xQueueAlarm;


void startup() {
  ssl.setCACert(CA_CERT);
  const char *ssl_protos[] = {"mqtt"};
  ssl.setAlpnProtocols(ssl_protos);
  tbloggerCb = &onTbLogger;
  xQueueAlarm = xQueueCreate( 10, sizeof( struct AlarmMessage ) );

  if(xSemaphoreSerialCoMCUWrite == NULL){xSemaphoreSerialCoMCUWrite = xSemaphoreCreateMutex();}
  if(xSemaphoreSerialCoMCURead == NULL){xSemaphoreSerialCoMCURead = xSemaphoreCreateMutex();}
  if(xSemaphoreUDPLogger == NULL){xSemaphoreUDPLogger = xSemaphoreCreateMutex();}
  if(xSemaphoreSettings == NULL){xSemaphoreSettings = xSemaphoreCreateMutex();}
  if(xSemaphoreConfig == NULL){xSemaphoreConfig = xSemaphoreCreateMutex();}
  if(xSemaphoreConfigCoMCU == NULL){xSemaphoreConfigCoMCU = xSemaphoreCreateMutex();}
  if(xSemaphoreTBSend == NULL){xSemaphoreTBSend = xSemaphoreCreateMutex();}
  if(xSemaphoreWSSend == NULL){xSemaphoreWSSend = xSemaphoreCreateMutex();}
  if(xSemaphoreCardLogger == NULL){xSemaphoreCardLogger = xSemaphoreCreateMutex();}

  // put your setup code here, to run once:
  Serial.begin(115200);

  config.logLev = 5;
  log_manager->add_logger(&serial_logger);
  #ifdef USE_WIFI_LOGGER
  log_manager->add_logger(&udp_logger);
  #endif
  log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);
  if(!SPIFFS.begin(true))
  {
    //configReset();
    configLoadFailSafe();
    log_manager->error(PSTR(__func__), PSTR("Problem with SPIFFS file system. Failsafe config was loaded.\n"));
  }
  else
  {
    log_manager->info(PSTR(__func__), PSTR("Loading config...\n"));
    configLoad();
    log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);
  }

  log_manager->info(PSTR(__func__), PSTR("Firmware version %s compiled on %s.\n"), CURRENT_FIRMWARE_VERSION, COMPILED);

  if(config.ECP < 60000){
    config.CC++;
    if(config.CC >= 60){
      config.SM = true;
      setAlarm(0, 1, 1000000, 50);
    }
  }
  log_manager->warn(PSTR(__func__), PSTR("ECP: %d, CC: %d, SM: %s\n"), config.ECP, config.CC, config.SM ? PSTR("ENABLED") : PSTR("DISABLED"));

  config.ECP = 0;
  configSave();
  
  #ifdef USE_SERIAL2
    log_manager->debug(PSTR(__func__), PSTR("Serial 2 - CoMCU Activated!\n"));
    Serial2.begin(115200, SERIAL_8N1, S2_RX, S2_TX);
  #endif

  if(!config.SM)
  {
    #ifdef USE_DISK_LOG
    #ifdef USE_SDCARD_LOG
    setupCardLogger();
    #endif
    #endif

    rtcUpdate(0);
  }

  log_manager->debug(PSTR(__func__), PSTR("Startup time: %s\n"), rtc.getDateTime().c_str());

  int tBytes = SPIFFS.totalBytes(); 
  int uBytes = SPIFFS.usedBytes();
  log_manager->verbose(PSTR(__func__), PSTR("SPIFFS total bytes: %d, used bytes: %d, free space: %d.\n"), tBytes, uBytes, tBytes-uBytes);

  if(xHandleWifiKeeper == NULL){
    xReturnedWifiKeeper = xTaskCreatePinnedToCore(wifiKeeperTR, PSTR("wifiKeeper"), STACKSIZE_WIFIKEEPER, NULL, 1, &xHandleWifiKeeper, 1);
    if(xReturnedWifiKeeper == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task wifiKeeper has been created.\n"));
    }
  }

  if(xHandleAlarm == NULL){
    xReturnedAlarm = xTaskCreatePinnedToCore(setAlarmTR, PSTR("setAlarm"), STACKSIZE_SETALARM, NULL, 1, &xHandleAlarm, 1);
    if(xReturnedAlarm == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task setAlarm has been created.\n"));
    }
  }


  setAlarm(0, 0, 3, 50);
}


void udawa(){
  if(FLAG_REBOOT_COUNTDOWN){
    if( (millis() - TIMER_FLAG_REBOOT_COUNTDOWN) >= (REBOOT_COUNTDOWN * 1000)){
      log_manager->warn(PSTR(__func__),PSTR("Device rebooting...\n"));
      ESP.restart();
    }
    else{
      long countdown = ((REBOOT_COUNTDOWN * 1000) - (millis() - TIMER_FLAG_REBOOT_COUNTDOWN)) / 1000;
      log_manager->warn(PSTR(__func__), PSTR("Reboot countdown: %ds\n"), countdown);
    }
  }

  if(FLAG_SAVE_CONFIG && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SAVE_CONFIG = false;
    configSave();
  }
  if(FLAG_SAVE_CONFIGCOMCU && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SAVE_CONFIGCOMCU = false;
    configCoMCUSave();
  }
  if(FLAG_SYNC_CONFIGCOMCU && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SYNC_CONFIGCOMCU = false;
    syncConfigCoMCU();
  }
  if(FLAG_SAVE_SETTINGS && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SAVE_SETTINGS = false;
    onSaveSettings();
  }
  if(FLAG_SAVE_STATES && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SAVE_STATES = false;
    onSaveStates();
  }
  if(FLAG_SYNC_CLIENT_ATTR_0 && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SYNC_CLIENT_ATTR_0 = false;
    syncClientAttr(0);
  }
  if(FLAG_SYNC_CLIENT_ATTR_1 && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SYNC_CLIENT_ATTR_1 = false;
    syncClientAttr(1);
  }
  if(FLAG_SYNC_CLIENT_ATTR_2 && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SYNC_CLIENT_ATTR_2 = false;
    syncClientAttr(2);
  }
  if(FLAG_UPDATE_SPIFFS && !FLAG_TB_OTA_ACTIVATED){
    FLAG_UPDATE_SPIFFS = false;
    updateSpiffs();
  }

  if(!FLAG_ECP_UPDATED && millis() > 30000 && !FLAG_TB_OTA_ACTIVATED){
    config.ECP = millis();
    FLAG_SAVE_CONFIG = true;
    FLAG_ECP_UPDATED = true;
  }

  if(!FLAG_SM_CLEARED && millis() > 60000 && !FLAG_TB_OTA_ACTIVATED){
    config.ECP = millis();
    config.CC = 0;
    FLAG_SAVE_CONFIG = true;
    FLAG_SM_CLEARED = true;
    log_manager->info(PSTR(__func__),PSTR("Safe mode cleared! Ready to reboot normally.\n"));
  }

  if(LAST_TB_CONNECTED != 0 && config.fIoT && tb.connected() && (millis() - LAST_TB_CONNECTED) > 10000 && 
    (millis() - LAST_TB_CONNECTED) < 11000  && !FLAG_TB_OTA_ACTIVATED){
    FLAG_SYNC_CLIENT_ATTR_1 = true;
    LAST_TB_CONNECTED = 0;
  }

  if(LAST_TB_CONNECTED != 0 && config.fIoT && tb.connected() && (millis() - LAST_TB_CONNECTED) > 20000 &&
    (millis() - LAST_TB_CONNECTED) < 21000 && !FLAG_TB_OTA_ACTIVATED){
    onTbConnectedCb();
    LAST_TB_CONNECTED = 0;
  }
}

void onTbLogger(const char *error){
  if (config.logLev == 6)
  {
    log_manager->verbose(PSTR(__func__), PSTR("%s.\n"), error);
  }
}

/// @brief Update callback that will be called as soon as the requested shared attributes, have been received.
/// The callback will then not be called anymore unless it is reused for another request
/// @param data Data containing the shared attributes that were requested and their current value
void processSharedAttributeRequest(const Shared_Attribute_Data &data) {
  if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
  {
    log_manager->verbose(PSTR(__func__), PSTR("Received shared attribute(s).\n"));
    if(config.logLev == 5){serializeJson(data, Serial); Serial.println();}
    xSemaphoreGive( xSemaphoreTBSend );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}

/// @brief Update callback that will be called as soon as the requested client-side attributes, have been received.
/// The callback will then not be called anymore unless it is reused for another request
/// @param data Data containing the client-side attributes that were requested and their current value
void processClientAttributeRequest(const Shared_Attribute_Data &data) {
  if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
  {
    log_manager->verbose(PSTR(__func__), PSTR("Received client attribute(s).\n"));
    if(config.logLev == 5){serializeJson(data, Serial); Serial.println();}
    xSemaphoreGive( xSemaphoreTBSend );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}

void processSharedAttributeUpdate(const Shared_Attribute_Data &data){
  if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
  {
    log_manager->verbose(PSTR(__func__), PSTR("Received shared attribute(s) update: \n"));
    if(config.logLev >= 5){
      String buffer;
      serializeJson(data, buffer); 
      log_manager->verbose(PSTR(__func__), PSTR("%s \n"), buffer.c_str());
    }
    if( xSemaphoreConfig != NULL ){
      if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
      {
        if(data["model"] != nullptr){strlcpy(config.model, data["model"].as<const char*>(), sizeof(config.model));}
        if(data["group"] != nullptr){strlcpy(config.group, data["group"].as<const char*>(), sizeof(config.group));}
        if(data["broker"] != nullptr){strlcpy(config.broker, data["broker"].as<const char*>(), sizeof(config.broker));}
        if(data["port"] != nullptr){config.port = data["port"].as<uint16_t>();}
        if(data["wssid"] != nullptr){strlcpy(config.wssid, data["wssid"].as<const char*>(), sizeof(config.wssid));}
        if(data["wpass"] != nullptr){strlcpy(config.wpass, data["wpass"].as<const char*>(), sizeof(config.wpass));}
        if(data["dssid"] != nullptr){strlcpy(config.dssid, data["dssid"].as<const char*>(), sizeof(config.dssid));}
        if(data["dpass"] != nullptr){strlcpy(config.dpass, data["dpass"].as<const char*>(), sizeof(config.dpass));}
        if(data["upass"] != nullptr){strlcpy(config.upass, data["upass"].as<const char*>(), sizeof(config.upass));}
        if(data["accTkn"] != nullptr){strlcpy(config.accTkn, data["accTkn"].as<const char*>(), sizeof(config.accTkn));}
        if(data["provDK"] != nullptr){strlcpy(config.provDK, data["provDK"].as<const char*>(), sizeof(config.provDK));}
        if(data["provDS"] != nullptr){strlcpy(config.provDS, data["provDS"].as<const char*>(), sizeof(config.provDS));}
        if(data["logLev"] != nullptr){config.logLev = data["logLev"].as<uint8_t>(); log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);}
        if(data["gmtOff"] != nullptr){config.gmtOff = data["gmtOff"].as<int>();}
        if(data["htU"] != nullptr){strlcpy(config.htU, data["htU"].as<const char*>(), sizeof(config.htU));}
        if(data["htP"] != nullptr){strlcpy(config.htP, data["htP"].as<const char*>(), sizeof(config.htP));}
        if(data["fWOTA"] != nullptr){config.fWOTA = data["fWOTA"].as<bool>();}
        if(data["fIface"] != nullptr){config.fIface = data["fIface"].as<bool>();}
        if(data["fIoT"] != nullptr){config.fIoT = data["fIoT"].as<bool>();}
        if(data["hname"] != nullptr){strlcpy(config.hname, data["hname"].as<const char*>(), sizeof(config.hname));}
        if(data["logIP"] != nullptr){strlcpy(config.logIP, data["logIP"].as<const char*>(), sizeof(config.logIP));}
        if(data["webApiKey"] != nullptr){strlcpy(config.webApiKey, data["webApiKey"].as<const char*>(), sizeof(config.webApiKey));}
        if(data["logPrt"] != nullptr){config.logPrt = data["logPrt"].as<uint16_t>();}
        xSemaphoreGive( xSemaphoreConfig );
      }
      else
      {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }

    if( xSemaphoreConfigCoMCU != NULL ){
      if( xSemaphoreTake( xSemaphoreConfigCoMCU, ( TickType_t ) 1000 ) == pdTRUE )
      {
        if(data["fP"] != nullptr){configcomcu.fP = data["fP"].as<bool>();}
        if(data["bFr"] != nullptr){configcomcu.bFr = data["bFr"].as<uint16_t>();}
        if(data["fB"] != nullptr){configcomcu.fB = data["fB"].as<bool>();}
        if(data["pBz"] != nullptr){configcomcu.pBz = data["pBz"].as<uint8_t>();}
        if(data["pLR"] != nullptr){configcomcu.pLR = data["pLR"].as<uint8_t>();}
        if(data["pLG"] != nullptr){configcomcu.pLG = data["pLG"].as<uint8_t>();}
        if(data["pLB"] != nullptr){configcomcu.pLB = data["pLB"].as<uint8_t>();}
        if(data["lON"] != nullptr){configcomcu.lON = data["lON"].as<uint8_t>();}
        xSemaphoreGive( xSemaphoreConfigCoMCU );
      }
      else
      {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }
    processSharedAttributeUpdateCb(data);
    FLAG_SYNC_CLIENT_ATTR_2 = true;
    setAlarm(0, 0, 1, 50);
    xSemaphoreGive( xSemaphoreTBSend );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}

void tbOtaFinishedCb(const bool& success){
  onMQTTUpdateEndCb();
  if(success){
    log_manager->info(PSTR(__func__), PSTR("IoT OTA update success!\n"));
  }else{
    log_manager->warn(PSTR(__func__), PSTR("IoT OTA update failed!\n"));
  }
  reboot(0);
}

void tbOtaProgressCb(const uint32_t& currentChunk, const uint32_t& totalChuncks){
  log_manager->verbose(PSTR(__func__), PSTR("IoT OTA Progress %.2f%%\n"), static_cast<float>(currentChunk * 100U) / totalChuncks);
}

#ifdef USE_WEB_IFACE
void ifaceTR(void *arg){
  #ifdef USE_ASYNC_WEB
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  #ifdef USE_INTERNAL_UI
    #ifdef USE_INTERNAL_UI_DIGEST
      web.serveStatic("/ui", SPIFFS, "/ui").setDefaultFile("index.html").setAuthentication(config.htU,config.htP).setAuthentication(config.htU,config.htP);
    #else
      web.serveStatic("/ui", SPIFFS, "/ui").setDefaultFile("index.html").setAuthentication(config.htU,config.htP);
    #endif
  #else
  web.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument jsonDoc(128);
    jsonDoc[PSTR("model")] = config.model;
    jsonDoc[PSTR("name")] = config.name;
    jsonDoc[PSTR("host")] = String(config.hname) + PSTR(".local");
    jsonDoc[PSTR("ipad")] = WiFi.localIP().toString();
    jsonDoc[PSTR("fwVer")] = CURRENT_FIRMWARE_VERSION;
    String jsonResponse = "";
    serializeJson(jsonDoc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });
  #endif
  web.begin();
  #ifdef USE_SPIFFS_LOG
    #ifdef USE_INTERNAL_UI_DIGEST
      web.serveStatic("/log", SPIFFS, "/www/log").setDefaultFile("recover.json").setAuthentication(config.htU,config.htP);
    #else
      web.serveStatic("/log", SPIFFS, "/www/log").setDefaultFile("recover.json");
    #endif
  #endif
  #ifdef USE_SDCARD_LOG
  if(!config.SM){
    #ifdef USE_INTERNAL_UI_DIGEST
      web.serveStatic("/log", SD, "/www/log").setDefaultFile("recover.json").setAuthentication(config.htU,config.htP);
    #else
      web.serveStatic("/log", SD, "/www/log").setDefaultFile("recover.json");
    #endif
  }
  #endif
  ws.onEvent(onWsEventCb);
  ws.enable(true);
  web.addHandler(&ws);

  while(true){
    ws.cleanupClients();
    vTaskDelay((const TickType_t) 1000 / portTICK_PERIOD_MS);
  }
  #endif

  #ifndef USE_ASYNC_WEB
  web.on(PSTR("/"), []() { webSendFile("/www/index.html", "text/html"); });
  web.on(PSTR("/runtime.js"), []() { webSendFile("/www/runtime.js", "application/javascript"); });
  web.on(PSTR("/polyfills.js"), []() { webSendFile("/www/polyfills.js", "application/javascript"); });
  web.on(PSTR("/main.js"), []() { webSendFile("/www/main.js", "application/javascript"); });
  web.on(PSTR("/styles.css"), []() { webSendFile("/www/styles.css", "text/css"); });
  web.on(PSTR("/assets/img/logo.svg"), []() { webSendFile("/www/assets/img/logo.svg", "image/svg+xml"); });
  web.on(PSTR("/favicon"), []() { webSendFile("/www/favicon.ico", "image/x-icon"); });

  web.begin();

  ws.begin();
  ws.onEvent(onWsEventCb);

  while(true){
    ws.loop();
    web.handleClient();
    vTaskDelay((const TickType_t) 3 / portTICK_PERIOD_MS);
  }
  #endif
}
#endif

#ifdef USE_WIFI_OTA
void wifiOtaTR(void *arg){
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
        setAlarm(0, 2, 1000, 50);
        onMQTTUpdateStartCb();
      })
      .onEnd([]()
      {
        log_manager->warn(PSTR(__func__),PSTR("Device rebooting...\n"));
        setAlarm(0, 0, 0, 1000);
        reboot(0);
      })
      .onProgress([](unsigned int progress, unsigned int total)
      {
        //log_manager->warn(PSTR(__func__),PSTR("OTA progress: %d/%d\n"), progress, total);
      })
      .onError([](ota_error_t error)
      {
        log_manager->warn(PSTR(__func__),PSTR("OTA Failed: %d\n"), error);
        reboot(0);
      }
    );
  }
  while(true){
    if(config.fWOTA){
      if( xSemaphoreUDPLogger != NULL && xSemaphoreTake( xSemaphoreUDPLogger, ( TickType_t ) 1000 ) == pdTRUE )
      {
        ArduinoOTA.handle();
        xSemaphoreGive( xSemaphoreUDPLogger );
      }
    }
    vTaskDelay((const TickType_t) 1000 / portTICK_PERIOD_MS);
  }
}
#endif

void processProvisionResponse(const Provision_Data &data)
{
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && !config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      constexpr char CREDENTIALS_TYPE[] PROGMEM = "credentialsType";
      constexpr char CREDENTIALS_VALUE[] PROGMEM = "credentialsValue";
      int jsonSize = JSON_STRING_SIZE(measureJson(data));
      char buffer[jsonSize];
      serializeJson(data, buffer, jsonSize);
      log_manager->verbose(PSTR(__func__),PSTR("Received device provision response: %s\n"), buffer);

      if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
        log_manager->warn(PSTR(__func__),PSTR("Provision response contains the error: (%s)\n"), data["errorMsg"].as<const char*>());
      }
      else
      {
        if (strncmp(data[CREDENTIALS_TYPE], PSTR("ACCESS_TOKEN"), strlen(PSTR("ACCESS_TOKEN"))) == 0) {
          strlcpy(config.accTkn, data[CREDENTIALS_VALUE].as<std::string>().c_str(), sizeof(config.accTkn));
          config.provSent = true;  
          FLAG_SAVE_CONFIG = true;
          log_manager->verbose(PSTR(__func__),PSTR("Access token provision response saved.\n"));
        }
        else if (strncmp(data[CREDENTIALS_TYPE], PSTR("MQTT_BASIC"), strlen(PSTR("MQTT_BASIC"))) == 0) {
          /*auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
          credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
          credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
          credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();*/
        }
        else {
          log_manager->warn(PSTR(__func__),PSTR("Unexpected provision credentialsType: (%s)\n"), data[CREDENTIALS_TYPE].as<const char*>());

        }
      }

      // Disconnect from the cloud client connected to the provision account, because it is no longer needed the device has been provisioned
      // and we can reconnect to the cloud with the newly generated credentials.
      if (tb.connected()) {
        tb.disconnect();
      }
      xSemaphoreGive( xSemaphoreTBSend );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}


void TBTR(void *arg){
  while(true){
    if(!config.provSent){
      if (tb.connect(config.broker, "provision", config.port)) {
        const Provision_Callback provisionCallback(Access_Token(), &processProvisionResponse, config.provDK, config.provDS, config.name);
        if(tb.Provision_Request(provisionCallback))
        {
          log_manager->info(PSTR(__func__),PSTR("Connected to provisioning server: %s:%d. Sending provisioning response: DK: %s, DS: %s, Name: %s \n"),  
            config.broker, config.port, config.provDK, config.provDS, config.name);
        }
      }
      else
      {
        log_manager->warn(PSTR(__func__),PSTR("Failed to connect to provisioning server: %s:%d\n"),  config.broker, config.port);
      }
      unsigned long timer = millis();
      while(true){
        tb.loop();
        if(config.provSent || (millis() - timer) > 10000){break;}
        vTaskDelay((const TickType_t)10 / portTICK_PERIOD_MS);
      }
    }
    else{
      if(!tb.connected())
      {
        log_manager->warn(PSTR(__func__),PSTR("IoT disconnected!\n"));
        onTbDisconnectedCb();
        log_manager->info(PSTR(__func__),PSTR("Connecting to broker %s:%d\n"), config.broker, config.port);
        uint8_t tbDisco = 0;
        while(!tb.connect(config.broker, config.accTkn, config.port, config.name)){  
          tbDisco++;
          log_manager->warn(PSTR(__func__),PSTR("Failed to connect to IoT Broker %s (%d)\n"), config.broker, tbDisco);
          if(tbDisco >= 10){
            if( xSemaphoreConfig != NULL ){
              if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
              {
                config.provSent = 0;
                xSemaphoreGive( xSemaphoreConfig );
              }
              else
              {
                log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
              }
            }
            tbDisco = 0;
            break;
          }
          vTaskDelay((const TickType_t)1000 / portTICK_PERIOD_MS);
        }

        if(tb.connected()){
          bool tbSharedUpdate_status = tb.Shared_Attributes_Subscribe(tbSharedAttrUpdateCb);
          bool tbClientRPC_status = tb.RPC_Subscribe(clientRPCCallbacks.cbegin(), clientRPCCallbacks.cend());
          tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION); 
          tb.Firmware_Send_State(PSTR("updated"));
          tb.Shared_Attributes_Request(fwCheckCb);

          LAST_TB_CONNECTED = millis();

          setAlarm(0, 0, 3, 50);
          log_manager->info(PSTR(__func__),PSTR("IoT Connected!\n"));
        }
      }
    }

    tb.loop();
    vTaskDelay((const TickType_t) 1 / portTICK_PERIOD_MS);
  }
}


void emitAlarm(int code){
  if(tb.connected()){
    StaticJsonDocument<32> doc;
    doc[PSTR("alarm")] = code;
    char buffer[32];
    serializeJson(doc, buffer);
    tbSendTelemetry(buffer);
  }
  emitAlarmCb(code);
  log_manager->error(PSTR(__func__), PSTR("%i\n"), code);
}

void rtcUpdate(long ts){
  #ifdef USE_HW_RTC
  bool rtcHwDetected = 0;
  if(!rtcHw.begin()){
    setAlarm(151, 1, 1, 5000);
    log_manager->debug(PSTR(__func__), PSTR("RTC Hardware not found; please update the device time manually. Any function that requires precise timing will malfunction! \n"));
  }
  else{
    rtcHwDetected = 1;
    rtcHw.setSquareWave(SquareWaveDisable);
  }
  #endif
  if(ts == 0){
    configTime(config.gmtOff, 0, "pool.ntp.org");
    bool ntpSuccess = timeClient.update();
    if (ntpSuccess){
      long epochTime = timeClient.getEpochTime();
      rtc.setTime(epochTime);
      log_manager->debug(PSTR(__func__), PSTR("Updated time via NTP: %s GMT Offset:%d (%d) \n"), rtc.getDateTime().c_str(), config.gmtOff, config.gmtOff / 3600);
      #ifdef USE_HW_RTC
      if(rtcHwDetected){
        log_manager->debug(PSTR(__func__), PSTR("Updating RTC HW from NTP...\n"));
        rtcHw.setDateTime(rtc.getHour(), rtc.getMinute(), rtc.getSecond(), rtc.getDay(), rtc.getMonth()+1, rtc.getYear(), rtc.getDayofWeek());
        log_manager->debug(PSTR(__func__), PSTR("Updated RTC HW from NTP with epoch %d | H:I:S W D-M-Y. -> %d:%d:%d %d %d-%d-%d\n"), 
        rtcHw.getEpoch(), rtc.getHour(), rtc.getMinute(), rtc.getSecond(), rtc.getDayofWeek(), rtc.getDay(), rtc.getMonth()+1, rtc.getYear());
      }
      #endif
    }else{
      #ifdef USE_HW_RTC
      if(rtcHwDetected){
        log_manager->debug(PSTR(__func__), PSTR("Updating RTC from RTC HW with epoch %d.\n"), rtcHw.getEpoch());
        rtc.setTime(rtcHw.getEpoch());
        log_manager->debug(PSTR(__func__), PSTR("Updated time via RTC HW: %s GMT Offset:%d (%d) \n"), rtc.getDateTime().c_str(), config.gmtOff, config.gmtOff / 3600);
      }
      #endif
    }
  }else{
      rtc.setTime(ts);
      log_manager->debug(PSTR(__func__), PSTR("Updated time via timestamp: %s\n"), rtc.getDateTime().c_str());
  }
}

void cbWiFiOnDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  log_manager->warn(PSTR(__func__),PSTR("WiFi %s disconnected!\n"), WiFi.SSID().c_str());
}

void cbWiFiOnGotIp(WiFiEvent_t event, WiFiEventInfo_t info)
{
  String ip = WiFi.localIP().toString();
  log_manager->warn(PSTR(__func__),PSTR("WiFi (%s) IP Assigned: %s!\n"), WiFi.SSID().c_str(), ip.c_str());

  MDNS.begin(config.hname);
  MDNS.addService("http", "tcp", 80);
  log_manager->info(PSTR(__func__),PSTR("Started MDNS on %s\n"), config.hname);

  timeClient.begin();

  rtcUpdate(0);

  #ifdef USE_WIFI_LOGGER  
  if(!config.SM){
    udp.begin(config.logPrt);
  }
  #endif

  #ifdef USE_WIFI_OTA
  if(config.fWOTA && xHandleWifiOta == NULL){
    xReturnedWifiOta = xTaskCreatePinnedToCore(wifiOtaTR, "wifiOta", STACKSIZE_WIFIOTA, NULL, 1, &xHandleWifiOta, 1);
    if(xReturnedWifiOta == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task wifiOta has been created.\n"));
    }
  }
  #endif

  if(config.fIoT && xHandleTB == NULL && !config.SM){
    xReturnedTB = xTaskCreatePinnedToCore(TBTR, "TB", STACKSIZE_TB, NULL, 1, &xHandleTB, 1);
    if(xReturnedTB == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task TB has been created.\n"));
    }
  }

  #ifdef USE_WEB_IFACE
  if(config.fIface && xHandleIface == NULL && !config.SM){
    xReturnedIface = xTaskCreatePinnedToCore(ifaceTR, "iface", STACKSIZE_IFACE, NULL, 1, &xHandleIface, 1);
    if(xReturnedIface == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task iface has been created.\n"));
    }
  }
  #endif

  if(rtc.getYear() < 2023){
    setAlarm(131, 1, 5, 1000);
  }

  setAlarm(0, 0, 3, 50);
}


void wifiKeeperTR(void *arg){
  log_manager->debug(PSTR(__func__),PSTR("Initializing wifi network...\n"));
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hname);
  WiFi.setAutoReconnect(true);
  if(!config.wssid || *config.wssid == 0x00 || strlen(config.wssid) > 48)
  {
    configLoadFailSafe();
    log_manager->warn(PSTR(__func__), PSTR("SSID too long or missing! Failsafe config was loaded.\n"));
  }
  wifiMulti.addAP(config.wssid, config.wpass);
  wifiMulti.addAP(config.dssid, config.dpass);

  WiFi.onEvent(cbWiFiOnDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(cbWiFiOnGotIp, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  
  while (true)
  {
    wifiMulti.run();
    vTaskDelay((const TickType_t) 30000 / portTICK_PERIOD_MS);
  }
}

#ifdef USE_WEB_IFACE
#ifdef USE_ASYNC_WEB
void onWsEventCb(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  StaticJsonDocument<DOCSIZE_MIN> root;
  JsonObject doc = root.to<JsonObject>();
  IPAddress clientIP = client->remoteIP();
  switch(type) {
    case WS_EVT_DISCONNECT:
      {
        log_manager->verbose(PSTR(__func__), PSTR("Client disconnected [%s]\n"), client->remoteIP().toString().c_str());
        // Remove client from maps
        clientAuthenticationStatus.erase(client->id());
        clientAuthAttemptTimestamps.erase(clientIP);

        if( xSemaphoreConfig != NULL ){
          if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
          {
            if(config.wsCount > 0){
              config.wsCount--;
            }
            xSemaphoreGive( xSemaphoreConfig );
          }
          else
          {
              log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
          }
        }
        log_manager->debug(PSTR(__func__), PSTR("ws [%u] disconnect. WsCount: %d\n"), client->id(), config.wsCount);
        doc["evType"] = (int)WS_EVT_DISCONNECT;
        doc["num"] = client->id();
        wsEventCb(doc);
      }
      break;
    case WS_EVT_CONNECT:
      {
        log_manager->verbose(PSTR(__func__), PSTR("New client arrived [%s]\n"), client->remoteIP().toString().c_str());
        // Initialize client as unauthenticated
        clientAuthenticationStatus[client->id()] = false;
        // Initialize timestamp for rate limiting
      }
      break;
    case WS_EVT_DATA:
      {
        DeserializationError err = deserializeJson(root, data);
        if(err != DeserializationError::Ok){
          return;
        }

        String dataLog;
        serializeJson(root, dataLog);
        log_manager->verbose(PSTR(__func__), PSTR("WS Data (%s): %s\n"), err.c_str(), dataLog.c_str());

        
        // If client is not authenticated, check credentials
        if(!clientAuthenticationStatus[client->id()]) {
          unsigned long currentTime = millis();
          unsigned long lastAttemptTime = clientAuthAttemptTimestamps[clientIP];

          if (currentTime - lastAttemptTime < config.rateLimitInterval) {
            // Too many attempts in short time, block this IP for blockInterval
            clientAuthAttemptTimestamps[clientIP] = currentTime + config.blockInterval - config.rateLimitInterval;
            log_manager->verbose(PSTR(__func__), PSTR("Too many authentication attempts. Blocking for %d seconds. Rate limit %d.\n"), config.blockInterval / 1000, config.rateLimitInterval);
            client->text(PSTR("{\"status\": {\"code\": 429, \"msg\": \"Too many authentication attempts. Please wait for 60 seconds.\"}}"));
            client->close();
            return;
          }
          
          clientAuthAttemptTimestamps[clientIP] = millis();

          if (err != DeserializationError::Ok) {
            client->text(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
            //client->close();
            return;
          }
          else{
            if(doc["salt"] == nullptr || doc["auth"] == nullptr){
              //client->text(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Unauthorized.\"}}"));
              //client->close();
              return;
            }

            const char* salt = doc["salt"];
            const char* auth = doc["auth"];

            char hashedApiKey[65];          
            hashApiKeyWithSalt(config.webApiKey, salt, hashedApiKey);
          
            if (strcmp(auth, hashedApiKey) == 0) {
              // Client authenticated successfully, update the status in the map
              clientAuthenticationStatus[client->id()] = true;
              if( xSemaphoreConfig != NULL ){
                if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
                {
                  config.wsCount++;
                  xSemaphoreGive( xSemaphoreConfig );
                }
                else
                {
                    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
                }
              }
              char broadcastModel[128];
              sprintf(broadcastModel, PSTR("{\"status\": {\"code\": 200, \"msg\": \"Authorized.\", \"model\": \"%s\"}}"), config.model);
              client->text(broadcastModel);
              log_manager->debug(PSTR(__func__), PSTR("ws [%u] authenticated. WsCount: %d\n"), client->id(), config.wsCount);
              doc["evType"] = (int)WS_EVT_CONNECT;
              doc["num"] = client->id();
              wsEventCb(doc);
            } else {
              // Unauthorized, you can choose to disconnect the client
              client->text(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Unauthorized.\"}}"));
              client->close();
            }

            // Update timestamp for rate limiting
            clientAuthAttemptTimestamps[clientIP] = currentTime;
          }
        }
        else {
          // The client is already authenticated, you can process the received data
          if (err == DeserializationError::Ok)
          {
            log_manager->debug(PSTR(__func__), PSTR("WS message parsing %s\n"), err.c_str());
            doc["evType"] = (int)WS_EVT_DATA;
            doc["num"] = client->id();
            wsEventCb(doc);
          }
          else
          {
            log_manager->warn(PSTR(__func__), PSTR("WS message parsing error: %s\n"), err.c_str());
          }
        }
      }
      break;
    case WS_EVT_ERROR:
      {
        log_manager->warn(PSTR(__func__), PSTR("ws [%u] error\n"), client->id());
        doc["evType"] = (int)WS_EVT_ERROR;
        doc["num"] = client->id();
        wsEventCb(doc);
      }
      break;	
  }
}
#endif
#ifndef USE_ASYNC_WEB
void onWsEventCb(uint8_t num, WStype_t type, uint8_t * data, size_t length){
  StaticJsonDocument<DOCSIZE_MIN> root;
  JsonObject doc = root.to<JsonObject>();
  switch(type) {
    case WStype_DISCONNECTED:
      {
        if( xSemaphoreConfig != NULL ){
          if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
          {
            if(config.wsCount > 0){
              config.wsCount--;
            }
            xSemaphoreGive( xSemaphoreConfig );
          }
          else
          {
              log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
          }
        }
        
        log_manager->debug(PSTR(__func__), PSTR("ws [%u] disconnect. WsCount: %d\n"), num, config.wsCount);
        doc["evType"] = (int)WStype_DISCONNECTED;
        doc["num"] = num;
        wsEventCb(doc);
      }
      break;
    case WStype_CONNECTED:
      {
        if( xSemaphoreConfig != NULL ){
          if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
          {
            config.wsCount++;
            xSemaphoreGive( xSemaphoreConfig );
          }
          else
          {
              log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
          }
        }
        log_manager->debug(PSTR(__func__), PSTR("ws [%u] connect. WsCount: %d\n"), num, config.wsCount);
        doc["evType"] = (int)WStype_CONNECTED;
        doc["num"] = num;
        wsEventCb(doc);
      }
      break;
    case WStype_TEXT:
      {
        DeserializationError err = deserializeJson(root, data);
        if (err == DeserializationError::Ok)
        {
          log_manager->debug(PSTR(__func__), PSTR("WS message parsing %s\n"), err.c_str());
          doc["evType"] = (int)WStype_TEXT;
          doc["num"] = num;
          wsEventCb(doc);
        }
        else
        {
          log_manager->warn(PSTR(__func__), PSTR("WS message parsing error: %s\n"), err.c_str());
        }
      }
      break;
    case WStype_ERROR:
      {
        log_manager->warn(PSTR(__func__), PSTR("ws [%u] error\n"), num);
        doc["evType"] = (int)WStype_ERROR;
        doc["num"] = num;
        wsEventCb(doc);
      }
      break;	
  }
}
#endif
#endif

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
  if( xQueueAlarm != NULL ){
    AlarmMessage alarmMsg;
    alarmMsg.code = code; alarmMsg.color = color; alarmMsg.blinkCount = blinkCount; alarmMsg.blinkDelay = blinkDelay;
    if( xQueueSend( xQueueAlarm, &alarmMsg, ( TickType_t ) 1000 ) != pdPASS )
    {
        log_manager->debug(PSTR(__func__), PSTR("Failed to set alarm. Queue is full. \n"));
    }
  }
}

void setAlarmTR(void *arg){
  while(true){
    if( xQueueAlarm != NULL ){
      AlarmMessage alarmMsg;
      if( xQueueReceive( xQueueAlarm,  &( alarmMsg ), ( TickType_t ) 100 ) == pdPASS )
      {
        #ifdef USE_SERIAL2
        setLed(alarmMsg.color, 1, alarmMsg.blinkCount, alarmMsg.blinkDelay);
        setBuzzer(alarmMsg.blinkCount, alarmMsg.blinkDelay);
        #endif
        if(alarmMsg.code > 0){
          emitAlarm(alarmMsg.code);
        }
        vTaskDelay((const TickType_t) (alarmMsg.blinkCount * alarmMsg.blinkDelay) / portTICK_PERIOD_MS);
      }
    }
    vTaskDelay((const TickType_t) 100 / portTICK_PERIOD_MS);
  }
}

void reboot(int countdown = 0)
{
  log_manager->info(PSTR(__func__),PSTR("Device rebot scheduled in %ds\n"), countdown);
  if(countdown > 0){
    TIMER_FLAG_REBOOT_COUNTDOWN = millis();
    REBOOT_COUNTDOWN = countdown;
    FLAG_REBOOT_COUNTDOWN = true;
  }
  else{
    log_manager->warn(PSTR(__func__),PSTR("Device rebooting...\n"));
    /*esp_task_wdt_init(1,true);
    esp_task_wdt_add(NULL);
    while(true);*/
    ESP.restart();
  }
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
  if( xSemaphoreConfig != NULL ){
    if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
    {
      File file;
      file = SPIFFS.open(configFile, FILE_WRITE);
      if (!file) {
        log_manager->warn(PSTR(__func__),PSTR("Failed to create config file. Config reset is cancelled.\n"));
        file.close();
        xSemaphoreGive( xSemaphoreConfig );
        return;
      }

      StaticJsonDocument<DOCSIZE> doc;

      char dv[16];
      sprintf(dv, "%s", getDeviceId());
      doc["ECP"] = 0;
      doc["CC"] = 0;
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

      doc["logIP"] = "255.255.255.255";
      doc["logPrt"] = 29514;

      doc["webApiKey"] = webApiKey;

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
      xSemaphoreGive( xSemaphoreConfig );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void configLoadFailSafe()
{
  if( xSemaphoreConfig != NULL ){
    if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
    {
      char dv[16];
      sprintf(dv, "%s", getDeviceId());
      strlcpy(config.hwid, dv, sizeof(config.hwid));

      config.ECP = 0;
      config.CC = 0;
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
      strlcpy(config.webApiKey, webApiKey, sizeof(config.webApiKey));
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
      strlcpy(config.logIP, "255.255.255.255", sizeof(config.logIP));
      config.logPrt = 29514;

      xSemaphoreGive( xSemaphoreConfig );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void configLoad()
{
  if( xSemaphoreConfig != NULL ){
    if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE )
    {
      log_manager->info(PSTR(__func__),PSTR("Loading config file.\n"));
      File file = SPIFFS.open(configFile, FILE_READ);
      if(file.size() > 1)
      {
        log_manager->info(PSTR(__func__),PSTR("Config file size is normal: %d, trying to fit it in %d docsize.\n"), file.size(), DOCSIZE);
      }
      else
      {
        file.close();
        log_manager->warn(PSTR(__func__),PSTR("Config file size is abnormal: %d. Closing file and trying to reset...\n"), file.size());
        xSemaphoreGive( xSemaphoreConfig );
        configReset();
        return;
      }

      StaticJsonDocument<DOCSIZE> doc;
      DeserializationError error = deserializeJson(doc, file);

      if(error)
      {
        log_manager->warn(PSTR(__func__),PSTR("Failed to load config file! (%s - %s - %d). Falling back to failsafe.\n"), configFile, error.c_str(), file.size());
        file.close();
        xSemaphoreGive( xSemaphoreConfig );
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
        if(doc["ECP"] != nullptr){config.ECP = doc["ECP"].as<unsigned long>();}
        if(doc["CC"] != nullptr){config.CC = doc["CC"].as<int>();}
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
        if(doc["logLev"] != nullptr){config.logLev = doc["logLev"].as<uint8_t>(); log_manager->set_log_level(PSTR("*"), (LogLevel) config.logLev);}
        if(doc["gmtOff"] != nullptr){config.gmtOff = doc["gmtOff"].as<int>();}
        if(doc["fIoT"] != nullptr){config.fIoT = doc["fIoT"].as<bool>();}
        if(doc["htU"] != nullptr){strlcpy(config.htU, doc["htU"].as<const char*>(), sizeof(config.htU));}
        if(doc["htP"] != nullptr){strlcpy(config.htP, doc["htP"].as<const char*>(), sizeof(config.htP));}
        if(doc["fWOTA"] != nullptr){config.fWOTA = doc["fWOTA"].as<bool>();}
        if(doc["fIface"] != nullptr){config.fIface = doc["fIface"].as<bool>();}
        if(doc["hname"] != nullptr){strlcpy(config.hname, doc["hname"].as<const char*>(), sizeof(config.hname));}
        if(doc["logIP"] != nullptr){strlcpy(config.logIP, doc["logIP"].as<const char*>(), sizeof(config.logIP));}
        if(doc["webApiKey"] != nullptr){strlcpy(config.webApiKey, doc["webApiKey"].as<const char*>(), sizeof(config.webApiKey));}
        if(doc["logPrt"] != nullptr){config.logPrt = doc["logPrt"].as<uint16_t>();}

        int jsonSize = JSON_STRING_SIZE(measureJson(doc));
        char buffer[jsonSize];
        serializeJson(doc, buffer, jsonSize);
        log_manager->verbose(PSTR(__func__),PSTR("Loaded config: %s.\n"), buffer);
      }
      file.close();
      xSemaphoreGive( xSemaphoreConfig );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void configSave()
{
  if( xSemaphoreConfig != NULL ){
    if( xSemaphoreTake( xSemaphoreConfig, ( TickType_t ) 1000 ) == pdTRUE ) {
      if(!SPIFFS.remove(configFile))
      {
        log_manager->warn(PSTR(__func__),PSTR("Failed to delete the old configFile: %s\n"), configFile);
      }
      File file = SPIFFS.open(configFile, FILE_WRITE);
      if (!file)
      {
        file.close();
        xSemaphoreGive( xSemaphoreConfig );
        return;
      }

      StaticJsonDocument<DOCSIZE> doc;
      doc["ECP"] = config.ECP;
      doc["CC"] = config.CC;
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
      doc["webApiKey"] = config.webApiKey;
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
      log_manager->verbose(PSTR(__func__), PSTR("Config saved successfully.\n"));
      xSemaphoreGive( xSemaphoreConfig );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}


void configCoMCUReset()
{
  if( xSemaphoreConfigCoMCU != NULL ){
    if( xSemaphoreTake( xSemaphoreConfigCoMCU, ( TickType_t ) 1000 ) == pdTRUE )
    {
      SPIFFS.remove(configFileCoMCU);
      File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
      if (!file) {
        file.close();
        xSemaphoreGive( xSemaphoreConfigCoMCU );
        return;
      }

      StaticJsonDocument<DOCSIZE> doc;

      doc["fP"] = false;

      doc["bFr"] = 0;
      doc["fB"] = 1;

      doc["pBz"] = 2;
      doc["pLR"] = 3;
      doc["pLG"] = 5;
      doc["pLB"] = 6;
      doc["lON"] = 0;

      serializeJson(doc, file);
      file.close();

      log_manager->info(PSTR(__func__),PSTR("ConfigCoMCU hard reset!\n")); 
      xSemaphoreGive( xSemaphoreConfigCoMCU );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void configCoMCULoad()
{
  if( xSemaphoreConfigCoMCU != NULL ){
    if( xSemaphoreTake( xSemaphoreConfigCoMCU, ( TickType_t ) 1000 ) == pdTRUE )
    {
      File file = SPIFFS.open(configFileCoMCU);
      StaticJsonDocument<DOCSIZE> doc;
      DeserializationError error = deserializeJson(doc, file);

      if(error)
      {
        file.close();
        xSemaphoreGive( xSemaphoreConfigCoMCU );
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
      xSemaphoreGive( xSemaphoreConfigCoMCU );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void configCoMCUSave()
{
  if( xSemaphoreConfigCoMCU != NULL ){
    if( xSemaphoreTake( xSemaphoreConfigCoMCU, ( TickType_t ) 1000 ) == pdTRUE )
    {
      if(!SPIFFS.remove(configFileCoMCU))
      {
        log_manager->warn(PSTR(__func__),PSTR("Failed to delete the old configFileCoMCU: %s\n"), configFileCoMCU);
      }
      File file = SPIFFS.open(configFileCoMCU, FILE_WRITE);
      if (!file)
      {
        file.close();
        xSemaphoreGive( xSemaphoreConfigCoMCU );
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
      log_manager->verbose(PSTR(__func__), PSTR("ConfigCoMCU saved successfully.\n"));
      xSemaphoreGive( xSemaphoreConfigCoMCU );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void syncConfigCoMCU()
{
  configCoMCULoad();
  if( xSemaphoreConfigCoMCU != NULL ){
    if( xSemaphoreTake( xSemaphoreConfigCoMCU, ( TickType_t ) 1000 ) == pdTRUE )
    {
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
      xSemaphoreGive( xSemaphoreConfigCoMCU );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
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

void serialWriteToCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, bool isRpc, int wait)
{
  if( xSemaphoreSerialCoMCUWrite != NULL ){
      /* See if we can obtain the semaphore.  If the semaphore is not
      available wait 10 ticks to see if it becomes free. */
      if( xSemaphoreTake( xSemaphoreSerialCoMCUWrite, ( TickType_t ) wait ) == pdTRUE )
      {
          /* We were able to obtain the semaphore and can now access the
          shared resource. */

          //long startMillis = millis();
          serializeJson(doc, Serial2);
          StringPrint stream;
          serializeJson(doc, stream);
          String result = stream.str();
          if(config.logLev == 6){
            log_manager->verbose(PSTR(__func__),PSTR("Sent to CoMCU: %s\n"), result.c_str());
          }
          
          if(isRpc)
          {
            vTaskDelay((const TickType_t) wait / portTICK_PERIOD_MS);
            doc.clear();
            serialReadFromCoMcu(doc, wait);
          }
          //log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);

          /* We have finished accessing the shared resource.  Release the
          semaphore. */
          xSemaphoreGive( xSemaphoreSerialCoMCUWrite );
      }
      else
      {
          /* We could not obtain the semaphore and can therefore not access
          the shared resource safely. */
          log_manager->debug(PSTR(__func__), PSTR("Unable to get semaphore.\n"));
      }
  }
}

void serialReadFromCoMcu(StaticJsonDocument<DOCSIZE_MIN> &doc, int wait)
{
  if( xSemaphoreSerialCoMCURead != NULL ){
      /* See if we can obtain the semaphore.  If the semaphore is not
      available wait 10 ticks to see if it becomes free. */
      if( xSemaphoreTake( xSemaphoreSerialCoMCURead, ( TickType_t ) 10000 ) == pdTRUE )
      {
          /* We were able to obtain the semaphore and can now access the
          shared resource. */

          //long startMillis = millis();
          StringPrint stream;
          String result;
          ReadLoggingStream loggingStream(Serial2, stream);
          DeserializationError err = deserializeJson(doc, loggingStream);
          result = stream.str();
          if (err == DeserializationError::Ok)
          {
            if(config.logLev == 6){
              log_manager->verbose(PSTR(__func__),PSTR("Received from CoMCU: %s\n"), result.c_str());
            }
          }
          else
          {
            log_manager->verbose(PSTR(__func__),PSTR("Serial2CoMCU DeserializeJson() returned: %s, content: %s\n"), err.c_str(), result.c_str());
            return;
          }
          //log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);

          /* We have finished accessing the shared resource.  Release the
          semaphore. */
          xSemaphoreGive( xSemaphoreSerialCoMCURead );
      }
      else
      {
          /* We could not obtain the semaphore and can therefore not access
          the shared resource safely. */
          log_manager->debug(PSTR(__func__), PSTR("Unable to get semaphore.\n"));
      }
  }
}


void readSettings(StaticJsonDocument<DOCSIZE_SETTINGS> &doc, const char* path)
{
  if( xSemaphoreSettings != NULL ){
    if( xSemaphoreTake( xSemaphoreSettings, ( TickType_t ) 1000 ) == pdTRUE )
    {
      File file = SPIFFS.open(path);
      DeserializationError error = deserializeJson(doc, file);

      if(error)
      {
        file.close();
        xSemaphoreGive( xSemaphoreSettings );
        return;
      }
      file.close();
      xSemaphoreGive( xSemaphoreSettings );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void writeSettings(StaticJsonDocument<DOCSIZE_SETTINGS> &doc, const char* path)
{
  if( xSemaphoreSettings != NULL ){
    if( xSemaphoreTake( xSemaphoreSettings, ( TickType_t ) 1000 ) == pdTRUE )
    {
      SPIFFS.remove(path);
      File file = SPIFFS.open(path, FILE_WRITE);
      if (!file)
      {
        file.close();
        xSemaphoreGive( xSemaphoreSettings );
        return;
      }
      serializeJson(doc, file);
      file.close();
      xSemaphoreGive( xSemaphoreSettings );
    }
    else
    {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
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

#ifdef USE_WIFI_LOGGER
void ESP32UDPLogger::log_message(const char *tag, LogLevel level, const char *fmt, va_list args) {
    if ((int)level > config.logLev || !WiFi.isConnected()) {
        return;
    }

    const int bufferSize = 256; // Adjust this size based on your requirements
    char data[bufferSize];
    int len = vsnprintf(data, bufferSize, fmt, args);
    if (len >= bufferSize) {
        // The message is larger than the buffer size; handle accordingly
        len = bufferSize - 1; // Limit length to buffer size
    }
    
    char logMessage[bufferSize + 64]; // Adjust extra space based on your concatenated string length
    snprintf(logMessage, sizeof(logMessage), "%c~[%s] %s~%s",
             get_error_char(level), config.name, tag, data);

    if (udp.beginPacket(config.logIP, config.logPrt)) {
        udp.write((uint8_t*)logMessage, strlen(logMessage));
        udp.endPacket();
    } else {
        log_manager->warn(PSTR(__func__), PSTR("Could not begin UDP packet.\n"));
    }
}
#endif

#ifdef USE_WEB_IFACE
#ifndef USE_ASYNC_WEB
void webSendFile(String path, String type){
  File file = SPIFFS.open(path.c_str(), FILE_READ);
  if(file){
    web.streamFile(file, type.c_str(), 200);
  }else{
    web.send(503, PSTR("text/plain"), PSTR("Server error."));
  }
  file.close();
}
#endif
#endif

void updateSpiffs()
{
    long startMillis = millis();

    BinDownloader _http;
    Serial.printf("Opening item %s\n", spiffsBinUrl );
    log_manager->info(PSTR(__func__), PSTR("Downloading SPIFFS: %s.\n"), spiffsBinUrl);

    _http.begin( spiffsBinUrl );

    const char* get_headers[] = { "Content-Length", "Content-type", "Accept-Ranges" };
    _http.collectHeaders( get_headers, sizeof(get_headers)/sizeof(const char*) );

    int64_t updateSize = 0;
    int httpCode = _http.GET();
    String contentType;

    if( httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY ) {
        updateSize = _http.getSize();
        contentType = _http.header( "Content-type" );
        String acceptRange = _http.header( "Accept-Ranges" );
        if( acceptRange == "bytes" ) {
            log_manager->info(PSTR(__func__), PSTR("This server supports resume!\n"));
        } else {
            log_manager->info(PSTR(__func__), PSTR("This server dose not supports resume!\n"));
        }
    } else {
        log_manager->info(PSTR(__func__), PSTR("Server responded with HTTP Status %s.\n"), httpCode);
        return;
    }

    // TODO: Not all streams respond with a content length.
    // TODO: Set updateSize to UPDATE_SIZE_UNKNOWN when content type is valid.

    // check updateSize and content type
    if( updateSize<=0 ) {
        log_manager->info(PSTR(__func__), PSTR("Response is empty! updateSize: %d, contentType: %s\n"), (int)updateSize, contentType.c_str());
        return;
    }

    log_manager->info(PSTR(__func__), PSTR("updateSize: %d, contentType: %s\n"), (int)updateSize, contentType.c_str());

    Stream* stream = _http.getStreamPtr();
    if( updateSize<=0 || stream == nullptr ) {
        log_manager->warn(PSTR(__func__), PSTR("HTTP Error.\n"));
        return;
    }

    // some network streams (e.g. Ethernet) can be laggy and need to 'breathe'
    if( !stream->available() ) {
        uint32_t timeout = millis() + 3000;
        while( stream->available() ) {
            if( millis()>timeout ) {
                log_manager->warn(PSTR(__func__), PSTR("Stream timed out!\n"));
                return;
            }
            vTaskDelay((const TickType_t)10 / portTICK_PERIOD_MS);
        }
    }

    // If using compression, the size is implicitely unknown
    size_t fwsize = updateSize;       // fw_size is unknown if we have a compressed image

    bool canBegin = Update.begin(updateSize, U_SPIFFS);

    if( !canBegin ) {
        log_manager->warn(PSTR(__func__), PSTR("Not enough space to begin OTA, partition size mismatch?\n"));
        Update.abort();
        return;
    }else{
      setAlarm(0, 3, 1000, 25);
    }

    Update.onProgress( [](size_t progress, size_t size) {
      log_manager->verbose(PSTR(__func__), PSTR("SPIFFS Updater: %d/%d\n"), (int)progress, (int)size);
    });

    Serial.printf("Begin SPIFFS OTA. This may take 2 - 5 mins to complete. Things might be quiet for a while.. Patience!\n");
    // Some activity may appear in the Serial monitor during the update (depends on Update.onProgress)
    size_t written = Update.writeStream(*stream);

    if ( written == fwsize ) {
        log_manager->info(PSTR(__func__), PSTR("Written : %d successfully. \n"), (int)written);
        updateSize = written; // flatten value to prevent overflow when checking signature
    } else {
        log_manager->warn(PSTR(__func__), PSTR("Written only : %d / %d. Premature end of stream?\n"), (int)written, (int)updateSize);
        Update.abort();
        FLAG_SAVE_CONFIG = true;
        FLAG_SAVE_SETTINGS = true;
        FLAG_SAVE_CONFIGCOMCU = true;
        return;
    }

    if (!Update.end()) {
        log_manager->warn(PSTR(__func__), PSTR("An Update Error Occurred: %d\n"), Update.getError());
        setAlarm(0, 0, 0, 1000);
        FLAG_SAVE_CONFIG = true;
        FLAG_SAVE_SETTINGS = true;
        FLAG_SAVE_CONFIGCOMCU = true;
        return;
    }
    if (Update.isFinished()) {
        log_manager->info(PSTR(__func__), PSTR("Update successfully completed.\n"));
        setAlarm(0, 0, 0, 1000);
        FLAG_SAVE_CONFIG = true;
        FLAG_SAVE_SETTINGS = true;
        FLAG_SAVE_CONFIGCOMCU = true;
    } else {
        FLAG_SAVE_CONFIG = true;
        FLAG_SAVE_SETTINGS = true;
        FLAG_SAVE_CONFIGCOMCU = true;
        log_manager->warn(PSTR(__func__), PSTR("Update not finished! Something went wrong!\n"));
    }

    log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);    
}

RPC_Response processConfigSave(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      FLAG_SAVE_CONFIG = true;
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
      doc[PSTR("configSave")] = 1;
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(doc);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("configSave"), 1);
}

RPC_Response processConfigCoMCUSave(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      FLAG_SAVE_CONFIGCOMCU = true;
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
      doc[PSTR("configCoMCUSave")] = 1;
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(doc);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("configCoMCUSave"), 1);
}

RPC_Response processSaveSettings(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      FLAG_SAVE_SETTINGS = true;
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
      doc[PSTR("saveSettings")] = 1;
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(doc);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("saveSettings"), 1);
}

RPC_Response processSetPanic(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      processSetPanicCb(data);
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(PSTR("setPanic"), configcomcu.fP);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("setPanic"), 1);
}

RPC_Response processUpdateSpiffs(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      FLAG_UPDATE_SPIFFS = true;
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
      doc[PSTR("updateSpiffs")] = 1;
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(doc);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("updateSpiffs"), 1);
}


RPC_Response processReboot(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      reboot();
      StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
      doc[PSTR("cdown")] = 20; 
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(doc);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("cdown"), 1);
}

RPC_Response processGenericClientRPC(const RPC_Data &data){
  String buffer;
  serializeJson(data, buffer);
  log_manager->verbose(PSTR(__func__), PSTR("Received generic client rpc: %s.\n"), buffer.c_str());
  return processGenericClientRPCCb(data);
}

void syncClientAttr(uint8_t direction){
  String ip = WiFi.localIP().toString();
  
  StaticJsonDocument<DOCSIZE_MIN> doc;
  char buffer[384];

  if(tb.connected() && (direction == 0 || direction == 1) ){
    doc[PSTR("ipad")] = ip;
    doc[PSTR("compdate")] = COMPILED;
    doc[PSTR("fmTitle")] = CURRENT_FIRMWARE_TITLE;
    doc[PSTR("fmVersion")] = CURRENT_FIRMWARE_VERSION;
    doc[PSTR("stamac")] = WiFi.macAddress();
    doc[PSTR("apmac")] = WiFi.softAPmacAddress();
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("flFree")] = ESP.getFreeSketchSpace();
    doc[PSTR("fwSize")] = ESP.getSketchSize();
    doc[PSTR("flSize")] = ESP.getFlashChipSize();
    doc[PSTR("dSize")] = (int)SPIFFS.totalBytes(); 
    doc[PSTR("dUsed")] = (int)SPIFFS.usedBytes();
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("sdkVer")] = ESP.getSdkVersion();
    doc[PSTR("model")] = config.model;
    doc[PSTR("name")] = config.name;
    doc[PSTR("group")] = config.group;
    doc[PSTR("broker")] = config.broker;
    doc[PSTR("port")] = config.port;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("wssid")] = config.wssid;
    doc[PSTR("ap")] = WiFi.SSID();
    doc[PSTR("wpass")] = config.wpass;
    doc[PSTR("dssid")] = config.dssid;
    doc[PSTR("dpass")] = config.dpass;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("upass")] = config.upass;
    doc[PSTR("accTkn")] = config.accTkn;
    doc[PSTR("webApiKey")] = config.webApiKey;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("provDK")] = config.provDK;
    doc[PSTR("provDS")] = config.provDS;
    doc[PSTR("logLev")] = config.logLev;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("gmtOff")] = config.gmtOff;
    doc[PSTR("bFr")] = configcomcu.bFr;
    doc[PSTR("fP")] = (int)configcomcu.fP;
    doc[PSTR("fB")] = (int)configcomcu.fB;
    doc[PSTR("fIoT")] = (int)config.fIoT;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("fWOTA")] = (int)config.fWOTA;
    doc[PSTR("fIface")] = (int)config.fIface;
    doc[PSTR("hname")] = config.hname;
    doc[PSTR("logIP")] = config.logIP;
    doc[PSTR("logPrt")] = config.logPrt;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("pBz")] = configcomcu.pBz;
    doc[PSTR("pLR")] = configcomcu.pLR;
    doc[PSTR("pLG")] = configcomcu.pLG;
    doc[PSTR("pLB")] = configcomcu.pLB;
    doc[PSTR("htU")] = config.htU;
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
    doc[PSTR("htP")] = config.htP;
    doc[PSTR("lON")] = configcomcu.lON;
    #ifdef USE_SDCARD_LOG
    doc[PSTR("crByte")] = config.cardByte;
    doc[PSTR("crSize")] = config.cardSize;
    doc[PSTR("crUsed")] = config.cardUsed;
    #endif
    serializeJson(doc, buffer);
    tbSendAttribute(buffer);
    doc.clear();
  }

  #ifdef USE_WEB_IFACE
  if(config.wsCount > 0 && (direction == 0 || direction == 2)){
    JsonObject attr = doc.createNestedObject("attr");
    attr[PSTR("ipad")] = ip.c_str();
    attr[PSTR("compdate")] = COMPILED;
    attr[PSTR("fmTitle")] = CURRENT_FIRMWARE_TITLE;
    attr[PSTR("fmVersion")] = CURRENT_FIRMWARE_VERSION;
    attr[PSTR("stamac")] = WiFi.macAddress();
    attr[PSTR("apmac")] = WiFi.softAPmacAddress();
    attr[PSTR("flFree")] = ESP.getFreeSketchSpace();
    attr[PSTR("fwSize")] = ESP.getSketchSize();
    attr[PSTR("flSize")] = ESP.getFlashChipSize();
    attr[PSTR("dSize")] = (int)SPIFFS.totalBytes(); 
    attr[PSTR("dUsed")] = (int)SPIFFS.usedBytes();
    attr[PSTR("sdkVer")] = ESP.getSdkVersion();
    #ifdef USE_SDCARD_LOG
    attr[PSTR("crByte")] = config.cardByte;
    attr[PSTR("crSize")] = config.cardSize;
    attr[PSTR("crUsed")] = config.cardUsed;
    #endif
    serializeJson(doc, buffer);
    wsBroadcastTXT(buffer);
    doc.clear();
    JsonObject cfg = doc.createNestedObject("cfg");
    cfg[PSTR("name")] = config.name;
    cfg[PSTR("model")] = config.model;
    cfg[PSTR("group")] = config.group;
    cfg[PSTR("ap")] = WiFi.SSID();
    cfg[PSTR("gmtOff")] = config.gmtOff;
    cfg[PSTR("fP")] = (int)configcomcu.fP;
    cfg[PSTR("fIoT")] = (int)config.fIoT;
    cfg[PSTR("hname")] = config.hname;
    serializeJson(doc, buffer);
    wsBroadcastTXT(buffer);
  }
  #endif

  onSyncClientAttrCb(direction);
}

bool tbSendAttribute(const char *buffer){
  bool res = false;
  int length = strlen(buffer);
  if (buffer[length - 1] != '}') {
      log_manager->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected() && config.accTkn != NULL){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      log_manager->verbose(PSTR(__func__), PSTR("Sending attribute to broker: %s\n"), buffer);
      res = tb.sendAttributeJSON(buffer);
      xSemaphoreGive( xSemaphoreTBSend );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return res;
}

bool tbSendTelemetry(const char * buffer){
  bool res = false;
  int length = strlen(buffer);
  if (buffer[length - 1] != '}') {
      log_manager->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected() && config.accTkn != NULL){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      log_manager->verbose(PSTR(__func__), PSTR("Sending telemetry to broker: %s\n"), buffer);
      res = tb.sendTelemetryJson(buffer); 
      xSemaphoreGive( xSemaphoreTBSend );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }   
  }
  return res;
}

RPC_Response processUpdateApp(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      if(xHandleAlarm != NULL){vTaskSuspend(xHandleAlarm);}
      #ifdef USE_WIFI_OTA
      if(xHandleWifiOta != NULL){vTaskSuspend(xHandleWifiOta);}
      #endif
      FLAG_TB_OTA_ACTIVATED = true;
      onMQTTUpdateStartCb();
      setAlarm(0, 3, 65000, 50);
      tb.Start_Firmware_Update(tbOtaCb);
      reboot();
      xSemaphoreGive( xSemaphoreTBSend );
      return RPC_Response(PSTR("updateApp"), 1);
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("updateApp"), 1);
}

void processFwCheckAttributeRequest(const Shared_Attribute_Data &data){
  if( xSemaphoreTBSend != NULL && WiFi.isConnected() && config.provSent && tb.connected()){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 1000 ) == pdTRUE )
    {
      if(data["fw_version"] != nullptr){
        log_manager->info(PSTR(__func__), PSTR("Firmware check local: %s vs cloud: %s\n"), CURRENT_FIRMWARE_VERSION, data["fw_version"].as<const char*>());
        if(strcmp(data["fw_version"].as<const char*>(), CURRENT_FIRMWARE_VERSION)){
          if(xHandleAlarm != NULL){vTaskSuspend(xHandleAlarm);}
          #ifdef USE_WIFI_OTA
          if(xHandleWifiOta != NULL){vTaskSuspend(xHandleWifiOta);}
          #endif
          FLAG_TB_OTA_ACTIVATED = true;
          onMQTTUpdateStartCb();
          setAlarm(0, 3, 65000, 50);
          tb.Start_Firmware_Update(tbOtaCb);
        }
      }
      xSemaphoreGive( xSemaphoreTBSend );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

#ifdef USE_WEB_IFACE
bool wsBroadcastTXT(const char *buffer){
  bool res = false;
  if(config.fIface && config.wsCount > 0){
    int length = strlen(buffer);
    if (buffer[length - 1] != '}') {
        log_manager->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
        return false;
    }
    if( xSemaphoreWSSend != NULL){
      if( xSemaphoreTake( xSemaphoreWSSend, ( TickType_t ) 1000 ) == pdTRUE )
      {
        #ifdef USE_ASYNC_WEB
        ws.cleanupClients(); // remove disconnected clients
        for(auto& it : clientAuthenticationStatus) {
            if(it.second) { // if the client is authenticated
                AsyncWebSocketClient* client = ws.client(it.first); // get the client using the client id
                if(client != nullptr) {
                    client->text(buffer);
                    if(config.logLev == 6){
                      log_manager->verbose(PSTR(__func__),PSTR("Broadcasted to websocket: %s\n"), buffer);
                    }
                }
            }
        }
        res = true;
        #endif
        #ifndef USE_ASYNC_WEB
        res = ws.broadcastTXT(buffer);
        #endif
        xSemaphoreGive( xSemaphoreWSSend );
      }
      else
      {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }
  }
  return res;
}

bool wsSendTXT(uint32_t id, const char *buffer){
  bool res = false;
  if(config.fIface && config.wsCount > 0){
    int length = strlen(buffer);
    if (buffer[length - 1] != '}') {
        log_manager->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
        return false;
    }
    if( xSemaphoreWSSend != NULL){
      if( xSemaphoreTake( xSemaphoreWSSend, ( TickType_t ) 1000 ) == pdTRUE )
      {
        #ifdef USE_ASYNC_WEB
        ws.text(id, buffer);
        if(config.logLev == 6){
          log_manager->verbose(PSTR(__func__),PSTR("Sent to websocket client %d: %s\n"), id, buffer);
        }
        res = true;
        #endif
        #ifndef USE_ASYNC_WEB
        res = ws.sendTXT(id, buffer);
        #endif
        xSemaphoreGive( xSemaphoreWSSend );
      }
      else
      {
        log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }
  }
  return res;
}
#endif

#ifdef USE_DISK_LOG
void setupCardLogger()
{
  #ifdef USE_SDCARD_LOG
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if(!SD.begin(SD_CS, sdSPI)){
    log_manager->warn(PSTR(__func__), PSTR("Card Mount Failed.\n"));
    setAlarm(130, 1, 10, 500);
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    log_manager->warn(PSTR(__func__), PSTR("No SD card attached.\n"));
    setAlarm(131, 1, 10, 500);
    return;
  }

  if( xSemaphoreSettings != NULL && xSemaphoreTake( xSemaphoreSettings, ( TickType_t ) 1000 ) == pdTRUE )
  {
    config.cardSize = SD.cardSize() / (1024 * 1024);
    config.cardByte = SD.totalBytes();
    config.cardUsed = SD.usedBytes();
    xSemaphoreGive(xSemaphoreSettings);
  }
  if(cardType == CARD_MMC){
    log_manager->debug(PSTR(__func__), PSTR("SD Card Type: NNC, size: %lluMB - byte: %llu - used: %llu\n"), 
      config.cardSize, config.cardByte, config.cardUsed);
  } else if(cardType == CARD_SD){
    log_manager->debug(PSTR(__func__), PSTR("SD Card Type: SDSC, size: %lluMB - byte: %llu - used: %llu\n"), 
      config.cardSize, config.cardByte, config.cardUsed);
  } else if(cardType == CARD_SDHC){
    log_manager->debug(PSTR(__func__), PSTR("SD Card Type: SDHC, size: %lluMB - byte: %llu - used: %llu\n"), 
      config.cardSize, config.cardByte, config.cardUsed);
  } else {
    log_manager->debug(PSTR(__func__), PSTR("SD Card Type: UNKNOWN, size: %lluMB - byte: %llu - used: %llu\n"), 
      config.cardSize, config.cardByte, config.cardUsed);
  }
  #endif
}
#endif

#ifdef USE_DISK_LOG
void writeCardLogger(StaticJsonDocument<DOCSIZE_MIN> &doc)
{
  if( xSemaphoreCardLogger != NULL && xSemaphoreTake( xSemaphoreCardLogger, ( TickType_t ) 10000 ) == pdTRUE )
  {
    unsigned long ts; String fileName;
    if(rtc.getYear() < 2023)
    {
      fileName = "/www/log/recover.json";
      ts = millis();
    }
    else
    {
      fileName = "/www/log/" + String(rtc.getYear()) + "-" + String(rtc.getMonth()+1) + "-" + String(rtc.getDay()) + ".json";
      ts = rtc.getEpoch();
    }
    #ifdef USE_SDCARD_LOG
    File file = SD.open(fileName.c_str(), FILE_APPEND);
    #endif
    #ifdef USE_SPIFFS_LOG
    File file = SPIFFS.open(fileName.c_str(), FILE_APPEND);
    #endif
    if (!file) {
      log_manager->warn(PSTR(__func__), PSTR("Failed to create the log file %s!\n"), fileName.c_str());
      setAlarm(132, 1, 10, 500);
      xSemaphoreGive( xSemaphoreCardLogger );
      return;
    }
    doc[PSTR("ts")] = ts;
    if (serializeJson(doc, file) == 0) {
      setAlarm(133, 1, 10, 500);
      log_manager->warn(PSTR(__func__), PSTR("Failed to write to the log file %s!\n"), fileName.c_str());
    }
    else
    {
      file.println();
    }

    file.close();
    xSemaphoreGive( xSemaphoreCardLogger );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}

void deleteAllLogFiles() {
  log_manager->debug(PSTR(__func__), PSTR("Deleting all log files in /www/log\n"));

  if( xSemaphoreCardLogger != NULL && xSemaphoreTake( xSemaphoreCardLogger, ( TickType_t ) 0 ) == pdTRUE )
  {
    const String logDirectory = "/www/log/";

    #ifdef USE_SDCARD_LOG 
    File root = SD.open(logDirectory);
    #endif
    #ifdef USE_SPIFFS_LOG
    File root = SPIFFS.open(logDirectory.c_str());
    #endif
    if (!root || !root.isDirectory()){
      log_manager->error(PSTR(__func__), PSTR("Failed to open log directory %s"), logDirectory.c_str());
      xSemaphoreGive( xSemaphoreCardLogger );
      return; 
    }

    while (File file = root.openNextFile()) {
      if (file.path() != nullptr) {
        #ifdef USE_SDCARD_LOG 
        if(SD.remove(file.path)){
          log_manager->verbose(PSTR(__func__), PSTR("File %s deleted"), file.path());
        } else {
          log_manager->error(PSTR(__func__), PSTR("Failed to delete file %s"), file.path());
        }
        #endif
        #ifdef USE_SPIFFS_LOG
        if(SPIFFS.remove(file.path())){
          log_manager->verbose(PSTR(__func__), PSTR("File %s deleted"), file.path());
        } else {
          log_manager->error(PSTR(__func__), PSTR("Failed to delete file %s"), file.path());
        }
        #endif
        
      }
      file.close();
    }

    xSemaphoreGive( xSemaphoreCardLogger );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}


void deleteLogFile(String fileName){
  log_manager->debug(PSTR(__func__), PSTR("Deleting log file: %s\n"), fileName.c_str());
  if( xSemaphoreCardLogger != NULL && xSemaphoreTake( xSemaphoreCardLogger, ( TickType_t ) 0 ) == pdTRUE )
  {
    String filePath = "/www/log/" + fileName;

    #ifdef USE_SDCARD_LOG
    if(SD.remove(filePath.c_str())){
      log_manager->verbose(PSTR(__func__), PSTR("File %s has been removed."), filePath.c_str());
    }
    else{
      log_manager->verbose(PSTR(__func__), PSTR("Failed to remove file %s."), filePath.c_str());
    }
    #endif
    #ifdef USE_SPIFFS_LOG
    if(SPIFFS.remove(filePath.c_str())){
      log_manager->verbose(PSTR(__func__), PSTR("File %s has been removed."), filePath.c_str());
    }else{
      log_manager->verbose(PSTR(__func__), PSTR("Failed to remove file %s."), filePath.c_str());
    }
    #endif

    xSemaphoreGive( xSemaphoreCardLogger );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}
#endif

#ifdef USE_WEB_IFACE
#ifdef USE_DISK_LOG
void wsStreamCardLogger(uint32_t id, String fileName)
{
  log_manager->debug(PSTR(__func__), PSTR("Sending log file of %s to %d!\n"), fileName.c_str(), id);
  if( xSemaphoreCardLogger != NULL && xSemaphoreTake( xSemaphoreCardLogger, ( TickType_t ) 0 ) == pdTRUE )
  {
    String filePath = "/www/log/" + fileName;

    #ifdef USE_SDCARD_LOG
    File file = SD.open(fileName.c_str(), FILE_READ);
    #endif
    #ifdef USE_SPIFFS_LOG
    File file = SPIFFS.open(filePath.c_str(), FILE_READ);
    #endif
    if (!file) {
      log_manager->warn(PSTR(__func__), PSTR("Failed to open the log file %s!\n"), filePath.c_str());
      xSemaphoreGive( xSemaphoreCardLogger );
      return;
    }

    while (true) {
      StaticJsonDocument<DOCSIZE_MIN> doc;
      char buffer[DOCSIZE_MIN];
      DeserializationError err = deserializeJson(doc, file);
      if (err) 
      {
        doc[PSTR("src")] = PSTR("card-end");
        serializeJson(doc, buffer);
        //wsSendTXT(id, buffer);
        wsBroadcastTXT(buffer);
        break;
      };
      doc[PSTR("src")] = PSTR("card");
      serializeJson(doc, buffer);
      //wsSendTXT(id, buffer);
      wsBroadcastTXT(buffer);
      vTaskDelay((const TickType_t) 50 / portTICK_PERIOD_MS);
    }

    file.close();

    xSemaphoreGive( xSemaphoreCardLogger );
  }
  else
  {
    log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
  }
}
#endif

#ifdef USE_ASYNC_WEB
void hashApiKeyWithSalt(const char* apiKey, const char* salt, char outputBuffer[65]) {
  // Convert input strings to UTF-8
  String apiKeyUtf8 = String(apiKey);
  String saltUtf8 = String(salt);

  // Calculate the HMAC
  byte hmac[32];
  const byte* key = (const byte*)apiKeyUtf8.c_str();
  size_t keyLength = apiKeyUtf8.length();
  const byte* message = (const byte*)saltUtf8.c_str();
  size_t messageLength = saltUtf8.length();
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, key, keyLength);
  mbedtls_md_hmac_update(&ctx, message, messageLength);
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  // Convert the hash to a hex string
  for (int i = 0; i < 32; i++) {
    sprintf(&outputBuffer[i*2], "%02x", (unsigned int)hmac[i]);
  }
}
#endif
#endif

} // namespace libudawa
#endif
