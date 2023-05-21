/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Firmware for Vanilla UDAWA Board (Starter Kit)
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/
#include "main.h"
#define S1_TX 32
#define S1_RX 4

using namespace libudawa;
Settings mySettings;

BaseType_t xReturnedWsSendTelemetry;
BaseType_t xReturnedRecSensors;
BaseType_t xReturnedPublishDevTel;

TaskHandle_t xHandleWsSendTelemetry = NULL;
TaskHandle_t xHandleRecSensors = NULL;
TaskHandle_t xHandlePublishDevTel = NULL;

void setup()
{
  processSharedAttributeUpdateCb = &attUpdateCb;
  onTbDisconnectedCb = &onTbDisconnected;
  onTbConnectedCb = &onTbConnected;
  processSetPanicCb = &setPanic;
  processGenericClientRPCCb = &genericClientRPC;
  emitAlarmCb = &onAlarm;
  onSyncClientAttrCb = &onSyncClientAttr;
  onSaveSettings = &saveSettings;
  #ifdef USE_WEB_IFACE
  wsEventCb = &onWsEvent;
  #endif
  onMQTTUpdateStartCb = &onMQTTUpdateStart;
  onMQTTUpdateEndCb = &onMQTTUpdateEnd;
  startup();
  loadSettings();
  if(!config.SM){
    syncConfigCoMCU();
  }
  if(String(config.model) == String("Generic")){
    strlcpy(config.model, "Vanilla", sizeof(config.model));
  }

  tb.setBufferSize(1024);

  #ifdef USE_WEB_IFACE
  xQueueWsPayloadMessage = xQueueCreate( 1, sizeof( struct WSPayload ) );
  #endif

  if(xHandleRecSensors == NULL && !config.SM){
    xReturnedRecSensors = xTaskCreatePinnedToCore(recSensorsTR, PSTR("recSensors"), STACKSIZE_RECSENSORS, NULL, 1, &xHandleRecSensors, 1);
    if(xReturnedRecSensors == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task recSensors has been created.\n"));
    }
  }

  if(xHandlePublishDevTel == NULL && !config.SM){
    xReturnedPublishDevTel = xTaskCreatePinnedToCore(publishDeviceTelemetryTR, PSTR("publishDevTel"), STACKSIZE_PUBLISHDEVTEL, NULL, 1, &xHandlePublishDevTel, 1);
    if(xReturnedPublishDevTel == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task publishDevTel has been created.\n"));
    }
  }

  #ifdef USE_WEB_IFACE
  if(xHandleWsSendTelemetry == NULL && !config.SM){
    xReturnedWsSendTelemetry = xTaskCreatePinnedToCore(wsSendTelemetryTR, PSTR("wsSendTelemetry"), STACKSIZE_WSSENDTELEMETRY, NULL, 1, &xHandleWsSendTelemetry, 1);
    if(xReturnedWsSendTelemetry == pdPASS){
      log_manager->warn(PSTR(__func__), PSTR("Task wsSendTelemetry has been created.\n"));
    }
  }
  #endif
}

void loop(){
  udawa();
  if(!config.SM){
    
  }
  vTaskDelay((const TickType_t) 1000 / portTICK_PERIOD_MS);
}

void recSensorsTR(void *arg){
  while (true)
  {
    #ifdef USE_WEB_IFACE
    if( xQueueWsPayloadMessage != NULL && (config.wsCount > 0))
    {
      WSPayload payload;
      payload.data = 0;
      if( xQueueSend( xQueueWsPayloadMessage, &payload, ( TickType_t ) 1000 ) != pdPASS )
      {
          log_manager->debug(PSTR(__func__), PSTR("Failed to fill WSPayload. Queue is full. \n"));
      }
    }
    #endif
    vTaskDelay((const TickType_t) 1000 / portTICK_PERIOD_MS);
  }
}

void loadSettings()
{
  long startMillis = millis();

  StaticJsonDocument<DOCSIZE_SETTINGS> doc;
  readSettings(doc, settingsPath);

  log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);
}

void saveSettings()
{
  long startMillis = millis();
  StaticJsonDocument<DOCSIZE_SETTINGS> doc;

  writeSettings(doc, settingsPath);

  log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);
}

void attUpdateCb(const Shared_Attribute_Data &data)
{
  long startMillis = millis();

  if( xSemaphoreSettings != NULL ){
    if( xSemaphoreTake( xSemaphoreSettings, ( TickType_t ) 1000 ) == pdTRUE )
    {
      //...code
      xSemaphoreGive( xSemaphoreSettings );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }

  log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);
}

void onTbConnected(){
 
}

void onTbDisconnected(){
 
}

void setPanic(const RPC_Data &data){
    long startMillis = millis();

    if(data[PSTR("st")] != nullptr){
        StaticJsonDocument<DOCSIZE_MIN> doc;
        doc[PSTR("method")] = PSTR("sCfg");
        if(strcmp(data[PSTR("st")], PSTR("ON")) == 0){
            doc[PSTR("fP")] = 1;
            configcomcu.fP = 1;
            //setAlarm(666, 1, 1, 10000);
        }
        else{
            doc[PSTR("fP")] = 0;
            configcomcu.fP = 0;
        }
        serialWriteToCoMcu(doc, 0);
    }

    log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);
}

RPC_Response genericClientRPC(const RPC_Data &data){
  if( xSemaphoreTBSend != NULL){
    if( xSemaphoreTake( xSemaphoreTBSend, ( TickType_t ) 0 ) == pdTRUE )
    {
      if(data[PSTR("cmd")] != nullptr){
          const char * cmd = data["cmd"].as<const char *>();
          log_manager->verbose(PSTR(__func__), PSTR("Received command: %s\n"), cmd);

          if(strcmp(cmd, PSTR("command")) == 0){
            
          }
      }  
      xSemaphoreGive( xSemaphoreTBSend );
    }
    else
    {
      log_manager->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return RPC_Response(PSTR("generic"), 1);
}

void deviceTelemetry(){
    if(config.provSent && tb.connected() && config.fIoT){
      StaticJsonDocument<128> doc;
      char buffer[128];
      
      doc[PSTR("uptime")] = millis(); 
      doc[PSTR("heap")] = heap_caps_get_free_size(MALLOC_CAP_8BIT); 
      doc[PSTR("rssi")] = WiFi.RSSI(); 
      doc[PSTR("dt")] = rtc.getEpoch(); 

      serializeJson(doc, buffer);
      tbSendAttribute(buffer);
    }
}

void onAlarm(int code){
  char buffer[32];
  StaticJsonDocument<32> doc;
  doc[PSTR("alarm")] = code;
  serializeJson(doc, buffer);
  wsBroadcastTXT(buffer);
}

void publishDeviceTelemetryTR(void * arg){
  unsigned long timerDeviceTelemetry = millis();
  while(true){
    unsigned long now = millis();
    if( (now - timerDeviceTelemetry) > mySettings.itD * 1000 ){
      deviceTelemetry();
      timerDeviceTelemetry = now;
    }
    
    vTaskDelay((const TickType_t) 300 / portTICK_PERIOD_MS);
  }
}

void onSyncClientAttr(uint8_t direction){
    long startMillis = millis();

    StaticJsonDocument<1024> doc;
    char buffer[1024];
    
    /*if(tb.connected() && (direction == 0 || direction == 1)){
      serializeJson(doc, buffer);
      tbSendAttribute(buffer);
      doc.clear();
    }

    #ifdef USE_WEB_IFACE
    if(config.wsCount > 0 && (direction == 0 || direction == 2)){
      serializeJson(doc, buffer);
      wsBroadcastTXT(buffer);
      doc.clear();
    }
    #endif
    */
    log_manager->verbose(PSTR(__func__), PSTR("Executed (%dms).\n"), millis() - startMillis);
}

#ifdef USE_WEB_IFACE
void onWsEvent(const JsonObject &doc){
  if(doc["evType"] == nullptr){
    log_manager->debug(PSTR(__func__), "Event type not found.\n");
    return;
  }
  int evType = doc["evType"].as<int>();


  if(evType == (int)WStype_CONNECTED){
    FLAG_SYNC_CLIENT_ATTR_2 = true;
  }
  if(evType == (int)WStype_DISCONNECTED){
      if(config.wsCount < 1){
          log_manager->debug(PSTR(__func__),PSTR("No WS client is active. \n"));
      }
  }
  else if(evType == (int)WStype_TEXT){
      if(doc["cmd"] == nullptr){
          log_manager->debug(PSTR(__func__), "Command not found.\n");
          return;
      }
      const char* cmd = doc["cmd"].as<const char*>();
      if(strcmp(cmd, (const char*) "attr") == 0){
        processSharedAttributeUpdate(doc);
        FLAG_SYNC_CLIENT_ATTR_1 = true;
      }
      else if(strcmp(cmd, (const char*) "saveSettings") == 0){
        FLAG_SAVE_SETTINGS = true;
      }
      else if(strcmp(cmd, (const char*) "configSave") == 0){
        FLAG_SAVE_CONFIG = true;
      }
      else if(strcmp(cmd, (const char*) "setPanic") == 0){
        doc[PSTR("st")] = configcomcu.fP ? "OFF" : "ON";
        processSetPanic(doc);
      }
      else if(strcmp(cmd, (const char*) "reboot") == 0){
        reboot();
      }
  }
}

void wsSendTelemetryTR(void *arg){
  while(true){
    if(config.fIface && config.wsCount > 0){
      char buffer[128];
      StaticJsonDocument<128> doc;
      JsonObject devTel = doc.createNestedObject("devTel");
      devTel[PSTR("heap")] = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      devTel[PSTR("rssi")] = WiFi.RSSI();
      devTel[PSTR("uptime")] = millis()/1000;
      devTel[PSTR("dt")] = rtc.getEpoch();
      devTel[PSTR("dts")] = rtc.getDateTime();
      serializeJson(doc, buffer);
      wsBroadcastTXT(buffer);
      doc.clear();
  
      if( xQueueWsPayloadMessage != NULL ){
        WSPayload payload;
        if( xQueueReceive( xQueueWsPayloadMessage,  &( payload ), ( TickType_t ) 1000 ) == pdPASS )
        {
          JsonObject sensors = doc.createNestedObject("sensors");
          sensors[PSTR("data")] = round2(payload.data);
          serializeJson(doc, buffer);
          wsBroadcastTXT(buffer);
          doc.clear();
        }
      } 
    }
    vTaskDelay((const TickType_t) 1000 / portTICK_PERIOD_MS);
  }
}

void onMQTTUpdateStart(){
  vTaskSuspend(xHandleRecSensors);
  vTaskSuspend(xHandleWsSendTelemetry);
  vTaskSuspend(xHandleIface);
  vTaskSuspend(xHandlePublishDevTel);
}

void onMQTTUpdateEnd(){
  
}
#endif