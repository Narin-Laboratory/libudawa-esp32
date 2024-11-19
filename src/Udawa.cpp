#include "Udawa.h"

Udawa::Udawa() : config(PSTR("/config.json")), _crashStateConfig(PSTR("/crash.json"))
  ,RTC(0)
  #ifdef USE_LOCAL_WEB_INTERFACE
  ,http(80) 
  ,ws(PSTR("/ws"))
  #endif
  #ifdef USE_IOT
  ,_mqttClient(_tcpClient), 
  tb(_mqttClient, IOT_MAX_MESSAGE_SIZE)
  #endif
  #ifdef USE_IOT_OTA
  ,_iotUpdaterFirmwareCheckCallback(
        createFirmwareCheckCallback(
            [this](const JsonObjectConst& data) {
                this->_processIoTUpdaterFirmwareCheckAttributesRequest(data);
            },
            std::array<const char*, 1>{FW_VER_KEY}
        )
    ),

  _iotUpdaterOTACallback(
    [this](const size_t& total, const size_t& progress) { 
        this->_iotUpdaterProgressCallback(total, progress);
    }, 
    [this](const bool& result) {
        this->_iotUpdaterUpdatedCallback(result);
    },
    CURRENT_FIRMWARE_TITLE, 
    CURRENT_FIRMWARE_VERSION, 
    &_iotUpdater, 
    IOT_FIRMWARE_FAILURE_RETRIES, 
    IOT_FIRMWARE_PACKET_SIZE
  ) 
  #endif
  {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);

    #ifdef USE_LOCAL_WEB_INTERFACE
    xSemaphoreWSBroadcast = NULL;
    if(xSemaphoreWSBroadcast == NULL){xSemaphoreWSBroadcast = xSemaphoreCreateMutex();}
    #endif

    #ifdef USE_IOT
    tb.setBufferSize(IOT_BUFFER_SIZE);
    if(iotState.xSemaphoreThingsboard == NULL){iotState.xSemaphoreThingsboard = xSemaphoreCreateMutex();}
    // Initialize Shared_Attribute_Callback with the correct arguments
    _thingsboardSharedAttributesUpdateCallback = Shared_Attribute_Callback([this](const JsonObjectConst &data) {
        this->_processThingsboardSharedAttributesUpdateWrapper(this, data); 
    });
    _thingsboardRPCRebootHandler = [this](const JsonVariantConst &data, JsonDocument &response) {
       return this->_processThingsboardRPCReboot(data, response);
    };
    _thingsboardRPCConfigSaveHandler = [this](const JsonVariantConst &data, JsonDocument &response) {
       return this->_processThingsboardRPCConfigSave(data, response);
    };
    #endif
}

void Udawa::begin(){
    _xQueueAlarm = xQueueCreate( 10, sizeof( struct AlarmMessage ) );
    logger->debug(PSTR(__func__), PSTR("Initializing LittleFS: %d\n"), config.begin());
    config.load();
    
    logger->setLogLevel((LogLevel)config.state.logLev);
    setAlarm(0, 0, 3, 50);
    #ifdef USE_WIFI_LOGGER
    wiFiLogger->setConfig(config.state.logIP, config.state.logPort, WIFI_LOGGER_BUFFER_SIZE);
    #endif

    #ifdef USE_I2C
    Wire.begin();
    Wire.setClock(400000);
    #endif


    JsonDocument doc;
    wiFiHelper.getAvailableWiFi(doc);
    File file = LittleFS.open("/WiFiList.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();


    wiFiHelper.addOnConnectedCallback(std::bind(&Udawa::_onWiFiConnected, this));
    wiFiHelper.addOnGotIPCallback(std::bind(&Udawa::_onWiFiGotIP, this));
    wiFiHelper.addOnDisconnectedCallback(std::bind(&Udawa::_onWiFiDisconnected, this));
    wiFiHelper.addOnAPNewClientIP(std::bind(&Udawa::_onWiFiAPNewClientIP, this));
    wiFiHelper.addOnAPStart(std::bind(&Udawa::_onWiFiAPStart, this));
    wiFiHelper.setInitState(config.state.fInit);
    wiFiHelper.begin(config.state.wssid, config.state.wpass, config.state.dssid, config.state.dpass, config.state.model, config.state.htP);

    logger->info(PSTR(__func__), PSTR("Firmware version %s compiled on %s.\n"), CURRENT_FIRMWARE_VERSION, COMPILED);
    
    _crashStateTruthKeeper(1);
    if(crashState.rtcp < 30000){
        crashState.crashCnt++;
        if(crashState.crashCnt >= 10){
            crashState.fSafeMode = true;
            logger->warn(PSTR(__func__), PSTR("** SAFEMODE ACTIVATED **\n"));
        }
    }
    logger->debug(PSTR(__func__), PSTR("Runtime Counter: %d, Crash Counter: %d, Safemode Status: %s\n"), crashState.rtcp, crashState.crashCnt, crashState.fSafeMode ? PSTR("ENABLED") : PSTR("DISABLED"));

    if(_xHandleAlarm == NULL){
      _xReturnedAlarm = xTaskCreatePinnedToCore(_alarmTaskRoutine, PSTR("alarmTaskRoutine"), ALARM_STACKSIZE, this, 1, &_xHandleAlarm, 1);
      if(_xReturnedAlarm == pdPASS){
        logger->warn(PSTR(__func__), PSTR("Task alarmTaskRoutine has been created.\n"));
      }
    }

    crashState.rtcp = 0;
    _crashStateTruthKeeper(2);
}

void Udawa::run(){
    unsigned long now = millis();

    wiFiHelper.run();

    #ifdef USE_WIFI_OTA
    ArduinoOTA.handle();
    #endif

    #ifdef USE_LOCAL_WEB_INTERFACE
    ws.cleanupClients();
    #endif

    if( !crashState.crashStateCheckedFlag && (now - crashState.crashStateCheckTimer) > 30000 ){
      crashState.fSafeMode = false;
      crashState.crashCnt = 0;
      logger->info(PSTR(__func__), PSTR("fSafeMode & Crash Counter cleared! Try to reboot normally.\n"));
      _crashStateTruthKeeper(2);
      crashState.crashStateCheckedFlag = true;
    }

    if( (now - crashState.lastRecordedDatetimeSavedTimer) > 60000 ){
      crashState.lastRecordedDatetimeSavedTimer = now;
      _crashStateTruthKeeper(2);
    }

    if(crashState.fPlannedReboot){
      if( now - crashState.plannedRebootTimer > 1000){
        if(crashState.plannedRebootCountDown <= 0){
          logger->warn(PSTR(__func__), PSTR("Reboting...\n"));
          ESP.restart();
        }
        crashState.plannedRebootCountDown--;
        logger->warn(PSTR(__func__), PSTR("Planned reboot in %d.\n"), crashState.plannedRebootCountDown);
        crashState.plannedRebootTimer = now;
      }
    }    
}

void Udawa::_setLEDBuzzer(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay){
  uint8_t r, g, b;
  switch (color)
  {
  //Auto by network
  case 0:
    #ifdef USE_IOT
    if(tb.connected()){
    #else
    if(false){
    #endif
      r = config.state.LEDOn == false ? true : false;
      g = config.state.LEDOn == false ? true : false;
      b = config.state.LEDOn;
    }
    else if(WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_MODE_STA){
      r = config.state.LEDOn == false ? true : false;
      g = config.state.LEDOn;
      b = config.state.LEDOn == false ? true : false;
    }
    else if(WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_MODE_AP && WiFi.softAPgetStationNum() > 0){
      r = config.state.LEDOn == false ? true : false;
      g = config.state.LEDOn;
      b = config.state.LEDOn == false ? true : false;
    }
    else{
      r = config.state.LEDOn;
      g = config.state.LEDOn == false ? true : false;
      b = config.state.LEDOn == false ? true : false;
    }
    break;
  //RED
  case 1:
    r = config.state.LEDOn;
    g = config.state.LEDOn == false ? true : false;
    b = config.state.LEDOn == false ? true : false;
    break;
  //GREEN
  case 2:
    r = config.state.LEDOn == false ? true : false;
    g = config.state.LEDOn;
    b = config.state.LEDOn == false ? true : false;
    break;
  //BLUE
  case 3:
    r = config.state.LEDOn == false ? true : false;
    g = config.state.LEDOn == false ? true : false;
    b = config.state.LEDOn;
    break;
  default:
    r = config.state.LEDOn;
    g = config.state.LEDOn;
    b = config.state.LEDOn;
  }

  if(isBlink){
    int32_t blinkCounter = 0;
    while (blinkCounter < blinkCount)
    {
      digitalWrite(config.state.pinLEDR, config.state.LEDOn == false ? true : false);
      digitalWrite(config.state.pinLEDG, config.state.LEDOn == false ? true : false);
      digitalWrite(config.state.pinLEDB, config.state.LEDOn == false ? true : false);
      digitalWrite(config.state.pinBuzz, HIGH);
      //logger->debug(PSTR(__func__), PSTR("Blinking LED and Buzzing, blinkDelay: %d, blinkCount: %d\n"), blinkDelay, blinkCount);
      vTaskDelay(pdMS_TO_TICKS(blinkDelay));
      digitalWrite(config.state.pinLEDR, r);
      digitalWrite(config.state.pinLEDG, g);
      digitalWrite(config.state.pinLEDB, b);
      digitalWrite(config.state.pinBuzz, LOW);
      //logger->debug(PSTR(__func__), PSTR("Stop Blinking LED and Buzzing.\n"));
      vTaskDelay(pdMS_TO_TICKS(blinkDelay));
      blinkCounter++;
    }
  }
  else{
    digitalWrite(config.state.pinLEDR, r);
    digitalWrite(config.state.pinLEDG, g);
    digitalWrite(config.state.pinLEDB, b);
  }
  
}

void Udawa::setAlarm(uint16_t code, uint8_t color, int32_t blinkCount, uint16_t blinkDelay){
  if( _xQueueAlarm != NULL ){
    AlarmMessage alarmMsg;
    alarmMsg.code = code; alarmMsg.color = color; alarmMsg.blinkCount = blinkCount; alarmMsg.blinkDelay = blinkDelay;
    if( xQueueSend( _xQueueAlarm, &alarmMsg, ( TickType_t ) 1000 ) != pdPASS )
    {
        logger->debug(PSTR(__func__), PSTR("Failed to set alarm. Queue is full. \n"));
    }
  }
}

void Udawa::_alarmTaskRoutine(void *arg){
  Udawa* self = static_cast<Udawa*>(arg);
  pinMode(self->config.state.pinLEDR, OUTPUT);
  pinMode(self->config.state.pinLEDG, OUTPUT);
  pinMode(self->config.state.pinLEDB, OUTPUT);
  pinMode(self->config.state.pinBuzz, OUTPUT);
  while(true){
    if( self->_xQueueAlarm != NULL ){
      AlarmMessage alarmMsg;
      if( xQueueReceive( self->_xQueueAlarm,  &( alarmMsg ), ( TickType_t ) 100 ) == pdPASS )
      {
        if(alarmMsg.code > 0){
          JsonDocument doc;
          JsonObject alarm = doc[PSTR("alarm")].to<JsonObject>();
          alarm[PSTR("code")] = alarmMsg.code;  

          #ifdef USE_LOCAL_WEB_INTERFACE
          self->wsBroadcast(doc);
          #endif
          
          #ifdef USE_IOT
          doc.clear();
          doc[PSTR("alarm")] = alarmMsg.code;
          self->iotSendTelemetry(doc);
          #endif
        }
        self->_setLEDBuzzer(alarmMsg.color, alarmMsg.blinkCount > 0 ? true : false, alarmMsg.blinkCount, alarmMsg.blinkDelay);
        self->logger->debug(PSTR(__func__), PSTR("Alarm code: %d, color: %d, blinkCount: %d, blinkDelay: %d\n"), alarmMsg.code, alarmMsg.color, alarmMsg.blinkCount, alarmMsg.blinkDelay);
        vTaskDelay((const TickType_t) (alarmMsg.blinkCount * alarmMsg.blinkDelay) / portTICK_PERIOD_MS);
      }
    }
    vTaskDelay((const TickType_t) 100 / portTICK_PERIOD_MS);
  }
}

void Udawa::_setFinit(bool fInit){
  config.state.fInit = fInit;
  config.save();

  #ifdef USE_LOCAL_WEB_INTERFACE
    if(config.state.fWeb && !crashState.fSafeMode){
      JsonDocument doc;
      doc[PSTR("cmd")] = PSTR("setFinishedSetup");
      doc[PSTR("fInit")] = config.state.fInit;
      String data;
      serializeJson(doc, data);
      wsBroadcast(data.c_str());
    }
  #endif
}

void Udawa::reboot(int countDown = 0){
  crashState.plannedRebootCountDown = countDown;
  crashState.fPlannedReboot = true;
}

void Udawa::_doInit(){
  logger->warn(PSTR(__func__), PSTR("Starting services setup protocol!\n"));
  if (!MDNS.begin(config.state.hname)) {
    logger->error(PSTR(__func__), PSTR("Error setting up MDNS responder!\n"));
  }
  else{
    logger->debug(PSTR(__func__), PSTR("mDNS responder started at %s\n"), config.state.hname);
  }

  MDNS.addService("http", "tcp", 80);

  #ifdef USE_WIFI_OTA
    if(config.state.fWOTA){
      logger->debug(PSTR(__func__), PSTR("Starting WiFi OTA at %s\n"), config.state.hname);
      ArduinoOTA.setHostname(config.state.hname);
      ArduinoOTA.setPasswordHash(config.state.upass);

      ArduinoOTA.onStart(std::bind(&Udawa::_onWiFiOTAStart, this));
      ArduinoOTA.onEnd(std::bind(&Udawa::_onWiFiOTAEnd, this));
      ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
          this->_onWiFiOTAProgress(progress, total);
      });
      ArduinoOTA.onError([this](ota_error_t error) {
          this->_onWiFiOTAError(error);
      });
      ArduinoOTA.begin();
    }
  #endif

  logger->debug(PSTR(__func__), PSTR("Starting Web Service...\n"));
  http.serveStatic("/", LittleFS, "/ui").setDefaultFile("index.html");
  http.serveStatic("/css/pico.blue.min.css", LittleFS, "/ui/css/pico.blue.min.css");
  http.serveStatic("/js/chart.min.js", LittleFS, "/ui/js/chart.min.js");

  ws.onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    this->_onWsEvent(server, client, type, arg, data, len);
  });

  http.addHandler(&ws);
  http.begin();
}

void Udawa::_startServices(){
  #ifdef USE_WIFI_LOGGER
    logger->addLogger(wiFiLogger);
    #endif
    rtcUpdate(0);
    #ifdef USE_WIFI_OTA
    if(config.state.fWOTA){
      logger->debug(PSTR(__func__), PSTR("Starting WiFi OTA at %s\n"), config.state.hname);
      ArduinoOTA.setHostname(config.state.hname);
      ArduinoOTA.setPasswordHash(config.state.upass);

      ArduinoOTA.onStart(std::bind(&Udawa::_onWiFiOTAStart, this));
      ArduinoOTA.onEnd(std::bind(&Udawa::_onWiFiOTAEnd, this));
      ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
          this->_onWiFiOTAProgress(progress, total);
      });
      ArduinoOTA.onError([this](ota_error_t error) {
          this->_onWiFiOTAError(error);
      });
      ArduinoOTA.begin();
    }
    #endif

    if (!MDNS.begin(config.state.hname)) {
        logger->error(PSTR(__func__), PSTR("Error setting up MDNS responder!\n"));
    }
    else{
        logger->debug(PSTR(__func__), PSTR("mDNS responder started at %s.\n"), config.state.hname);
    }

    MDNS.addService("http", "tcp", 80);

    #ifdef USE_LOCAL_WEB_INTERFACE
    if(config.state.fWeb && !crashState.fSafeMode){
      logger->debug(PSTR(__func__), PSTR("Starting Web Service...\n"));
      http.serveStatic("/", LittleFS, "/www").setDefaultFile("index.html");

      ws.onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
          this->_onWsEvent(server, client, type, arg, data, len);
      });

      http.addHandler(&ws);
      http.begin();
    }
    #endif

    #ifdef USE_IOT
    if(config.state.fIoT && iotState.xHandleIoT == NULL && !crashState.fSafeMode){
      iotState.xReturnedIoT = xTaskCreatePinnedToCore(_pvTaskCodeThingsboardTaskWrapper, PSTR("Thingsboard"), IOT_STACKSIZE_TB, this, 1, &iotState.xHandleIoT, 1);
      if(iotState.xReturnedIoT == pdPASS){
        logger->warn(PSTR(__func__), PSTR("Task Thingsboard has been created.\n"));
      }
    }
    #endif
}

void Udawa::_stopServices(){
  #ifdef USE_WIFI_LOGGER
    logger->addLogger(wiFiLogger);
    #endif
    
    #ifdef USE_WIFI_OTA
    if(config.state.fWOTA){
      logger->debug(PSTR(__func__), PSTR("Stopping WiFi OTA...\n"), config.state.hname);
      ArduinoOTA.end();
    }
    #endif

    logger->error(PSTR(__func__), PSTR("Stopping MDNS...\n"));
    MDNS.end();


    #ifdef USE_LOCAL_WEB_INTERFACE
    logger->error(PSTR(__func__), PSTR("Stopping HTTP...\n"));
    http.end();
    #endif
}

void Udawa::_onWiFiConnected(){
  setAlarm(0, 0, 3, 50);
}

void Udawa::_onWiFiDisconnected(){
  setAlarm(0, 0, 3, 50);
}

void Udawa::_onWiFiAPNewClientIP(){
  setAlarm(0, 0, 3, 50);
}

void Udawa::_onWiFiAPStart(){
  _doInit(); 
  setAlarm(0, 0, 3, 50);
}

void Udawa::_onWiFiGotIP(){
  _startServices();
  setAlarm(0, 0, 3, 50);
}

#ifdef USE_WIFI_OTA
void Udawa::_onWiFiOTAStart(){
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
    type = "sketch";
    } else { // U_SPIFFS
    type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    LittleFS.end();
    logger->debug(PSTR(""), PSTR("Start updating %s.\n"), type.c_str());
}

void Udawa::_onWiFiOTAEnd(){
    logger->debug(PSTR(__func__), PSTR("\n Finished.\n"));
}

void Udawa::_onWiFiOTAProgress(unsigned int progress, unsigned int total){
    logger->debug(PSTR(__func__), PSTR("Progress: %u%%\n"), (progress / (total / 100)));
}

void Udawa::_onWiFiOTAError(ota_error_t error){
    logger->error(PSTR(__func__), PSTR("Error[%u]: "), error);
    if (error == OTA_AUTH_ERROR) {
    logger->error(PSTR(""),PSTR("Auth Failed\n"));
    } else if (error == OTA_BEGIN_ERROR) {
    logger->error(PSTR(""),PSTR("Begin Failed\n"));
    } else if (error == OTA_CONNECT_ERROR) {
    logger->error(PSTR(""),PSTR("Connect Failed\n"));
    } else if (error == OTA_RECEIVE_ERROR) {
    logger->error(PSTR(""),PSTR("Receive Failed\n"));
    } else if (error == OTA_END_ERROR) {
    logger->error(PSTR(""),PSTR("End Failed\n"));
    }
}
#endif

#ifdef USE_LOCAL_WEB_INTERFACE
String Udawa::hmacSha256(String htP, String salt) {
  char outputBuffer[65]; // 2 characters per byte + null terminator

  // Convert input strings to UTF-8 byte arrays 
  std::vector<uint8_t> apiKeyUtf8(htP.begin(), htP.end());
  std::vector<uint8_t> saltUtf8(salt.begin(), salt.end());

  // Calculate the HMAC
  unsigned char hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // Set HMAC mode
  mbedtls_md_hmac_starts(&ctx, apiKeyUtf8.data(), apiKeyUtf8.size());
  mbedtls_md_hmac_update(&ctx, saltUtf8.data(), saltUtf8.size());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx); 

  // Convert the hash to a hex string (with leading zeros)
  for (int i = 0; i < 32; i++) {
    sprintf(&outputBuffer[i * 2], "%02x", hmac[i]);
  }

  // Null terminate the string
  outputBuffer[64] = '\0';
  return String(outputBuffer);  
}

void Udawa::wsBroadcast(const char *buffer){
  if(config.state.fWeb){
    if( xSemaphoreWSBroadcast != NULL){
      if( xSemaphoreTake( xSemaphoreWSBroadcast, ( TickType_t ) 1000 ) == pdTRUE )
      {
        ws.textAll(buffer);
        xSemaphoreGive( xSemaphoreWSBroadcast );
      }
      else
      {
        logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }
  }
}

void Udawa::wsBroadcast(JsonDocument &doc){
  if(config.state.fWeb){
    if( xSemaphoreWSBroadcast != NULL){
      if( xSemaphoreTake( xSemaphoreWSBroadcast, ( TickType_t ) 1000 ) == pdTRUE )
      {
        String buffer;
        serializeJson(doc, buffer);
        ws.textAll(buffer);
        xSemaphoreGive( xSemaphoreWSBroadcast );
      }
      else
      {
        logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
      }
    }
  }
}

void Udawa::_onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  IPAddress clientIP = client->remoteIP();
  switch(type) {
    case WS_EVT_DISCONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("Client disconnected.\n"));
        _wsClientAuthenticationStatus.erase(client->id());
        _wsClientAuthAttemptTimestamps.erase(clientIP);
        _wsClientSalts.erase(client->id());  // Remove salt on disconnect
        break;     
      }
      break;
    case WS_EVT_CONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("New client arrived [%s]\n"), clientIP.toString().c_str());
        _wsClientAuthenticationStatus[client->id()] = false;
        _wsClientAuthAttemptTimestamps[clientIP] = millis();

        if(config.state.fInit){
          // Generate a random salt
          unsigned char salt[16];
          mbedtls_entropy_context entropy;
          mbedtls_ctr_drbg_context ctr_drbg;
          const char *pers = "ws_salt";

          mbedtls_entropy_init(&entropy);
          mbedtls_ctr_drbg_init(&ctr_drbg);
          mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));
          mbedtls_ctr_drbg_random(&ctr_drbg, salt, sizeof(salt));
          mbedtls_ctr_drbg_free(&ctr_drbg);
          mbedtls_entropy_free(&entropy);

          String saltHex;
          for (int i = 0; i < sizeof(salt); i++) {
              char hex[3];
              sprintf(hex, "%02x", salt[i]);
              saltHex += hex;
          }
          
          // Store the salt for this client
          _wsClientSalts[client->id()] = saltHex;

          // Send salt to the client
          JsonDocument doc;
          JsonObject setSalt = doc[PSTR("setSalt")].to<JsonObject>();
          setSalt[PSTR("salt")] = saltHex;
          setSalt[PSTR("name")] = config.state.name;
          setSalt[PSTR("model")] = config.state.model;
          setSalt[PSTR("group")] = config.state.group;
          String message;
          serializeJson(doc, message);
          client->text(message);
          break;
        }
      }
      break;
    case WS_EVT_DATA:
      {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data);
        /*if(err != DeserializationError::Ok){
          logger->error(PSTR(__func__), PSTR("Failed to parse JSON.\n"));
          return;
        }*/
        
        serializeJsonPretty(doc, Serial);

        // If client is not authenticated, check credentials
        if(!_wsClientAuthenticationStatus[client->id()] && config.state.fInit) {
          logger->verbose(PSTR(__func__), PSTR("Client is NOT authenticated (%i) AND fInit is TRUE (%i)\n"), _wsClientAuthenticationStatus[client->id()], config.state.fInit);
          unsigned long currentTime = millis();
          unsigned long lastAttemptTime = _wsClientAuthAttemptTimestamps[clientIP];

          /**if (currentTime - lastAttemptTime < 1000) {
            // Too many attempts in short time, block this IP for blockInterval
            //_wsClientAuthAttemptTimestamps[clientIP] = currentTime + WS_BLOCKED_DURATION - WS_RATE_LIMIT_INTERVAL;
            logger->verbose(PSTR(__func__), PSTR("Too many authentication attempts. Blocking for %d seconds. Rate limit %d.\n"), WS_BLOCKED_DURATION / 1000, WS_RATE_LIMIT_INTERVAL);
            //client->close();
            return;
          }**/

          if (err != DeserializationError::Ok) {
            //client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
            //_wsClientAuthAttemptTimestamps[clientIP] = currentTime;
            return;
          }
          else{
            if(doc["salt"] == nullptr || doc["auth"] == nullptr){
              //client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
              //_wsClientAuthAttemptTimestamps[clientIP] = currentTime;
              return;
            }

            String clientAuth = doc["auth"].as<String>();
            String clientSalt = doc["salt"].as<String>();
            //logger->debug(PSTR(__func__), PSTR("\n\tserver: %s\n\tclient: %s\n\tkey: %s\n\tsalt: %s\n"), _auth.c_str(), auth.c_str(), config.state.htP, salt.c_str());
            
            if (_wsClientSalts[client->id()] == clientSalt) {
                // Compute expected HMAC with stored salt
                String expectedAuth = hmacSha256(htP, _wsClientSalts[client->id()]);

                // Check if the HMACs match
                logger->verbose(PSTR(__func__), PSTR("\nhtP:\t%s \n\nclientAuth:\t%s\n\nexpectedAuth:\t%s\n"), config.state.htP, clientAuth.c_str(), expectedAuth.c_str());
                if (clientAuth == expectedAuth) {
                    _wsClientAuthenticationStatus[client->id()] = true;
                    logger->verbose(PSTR(__func__), PSTR("Client authenticated successfully.\n"));
                    client->printf(PSTR("{\"status\": {\"code\": 200, \"msg\": \"Authorized.\", \"model\": \"%s\"}}"), config.state.model);
                } else {
                    logger->warn(PSTR(__func__), PSTR("Authentication failed.\n"));
                    client->printf(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Authorization failed.\", \"model\": \"%s\"}}"), config.state.model);
                }
            } else {
                logger->warn(PSTR(__func__), PSTR("Salt mismatch or expired.\n"));
                client->printf(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Salt mismatch or expired.\", \"model\": \"%s\"}}"), config.state.model);
            }
            // Update timestamp for rate limiting
            _wsClientAuthAttemptTimestamps[clientIP] = currentTime;
            return;
          }
        }
        else {
          // The client is already authenticated or fInit is false, you can process the received data
          //...
          String cmd = "";
          if(doc["cmd"] != nullptr){
            cmd = doc["cmd"].as<String>();
          }

          logger->verbose(PSTR(__func__), PSTR("Received command: %s \n"), cmd.c_str());

          if (cmd == "setConfig") {
            if(doc[PSTR("cfg")] != nullptr){
              if (doc[PSTR("cfg")][PSTR("wssid")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("wssid")].as<const char*>()) > 0) {
                strlcpy(config.state.wssid, doc[PSTR("cfg")][PSTR("wssid")].as<const char*>(), sizeof(config.state.wssid));
                logger->debug(PSTR(__func__), PSTR("wssid: %s\n"), doc[PSTR("cfg")]["wssid"].as<const char*>());
              }
              if (doc[PSTR("cfg")][PSTR("wpass")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("wpass")].as<const char*>()) > 0) {
                  strlcpy(config.state.wpass, doc[PSTR("cfg")][PSTR("wpass")].as<const char*>(), sizeof(config.state.wpass));
                  logger->debug(PSTR(__func__), PSTR("wpass: %s\n"), doc[PSTR("cfg")][PSTR("wpass")].as<const char*>());
              }
              if (doc[PSTR("cfg")][PSTR("gmtOff")] != nullptr) {
                  config.state.gmtOff = doc[PSTR("cfg")][PSTR("gmtOff")].as<int>();
                  logger->debug(PSTR(__func__), PSTR("gmtOff: %d\n"), config.state.gmtOff);  // Display as integer
              }
              if (doc[PSTR("cfg")][PSTR("group")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("group")].as<const char*>()) > 0) {
                  strlcpy(config.state.group, doc[PSTR("cfg")][PSTR("group")].as<const char*>(), sizeof(config.state.group));
                  logger->debug(PSTR(__func__), PSTR("group: %s\n"), doc[PSTR("cfg")][PSTR("group")].as<const char*>());
              }
              if (doc[PSTR("cfg")][PSTR("name")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("name")].as<const char*>()) > 0) {
                  strlcpy(config.state.name, doc[PSTR("cfg")][PSTR("name")].as<const char*>(), sizeof(config.state.name));
                  logger->debug(PSTR(__func__), PSTR("name: %s\n"), doc[PSTR("cfg")][PSTR("name")].as<const char*>());
              }
              if (doc[PSTR("cfg")][PSTR("hname")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("hname")].as<const char*>()) > 0) {
                  strlcpy(config.state.hname, doc[PSTR("cfg")][PSTR("hname")].as<const char*>(), sizeof(config.state.hname));
                  logger->debug(PSTR(__func__), PSTR("hname: %s\n"), doc[PSTR("cfg")][PSTR("hname")].as<const char*>());
              }
              if (doc[PSTR("cfg")][PSTR("htP")] != nullptr && strlen(doc[PSTR("cfg")][PSTR("htP")].as<const char*>()) > 0) {
                  strlcpy(config.state.htP, doc[PSTR("cfg")][PSTR("htP")].as<const char*>(), sizeof(config.state.htP));
                  logger->debug(PSTR(__func__), PSTR("htP: %s\n"), doc[PSTR("cfg")][PSTR("htP")].as<const char*>());
              }
            }
            config.save();
          }

          else if(cmd == "getConfig"){
            syncClientAttr(2);
          }

          else if(cmd == "getAvailableWiFi"){
            JsonDocument doc;
            JsonDocument WiFiList;
            File file = LittleFS.open("/WiFiList.json", FILE_READ);
            deserializeJson(WiFiList, file);
            file.close();
            doc[PSTR("cmd")] = PSTR("getAvailableWiFi");
            doc[PSTR("WiFiList")] = WiFiList;
            String data;
            serializeJson(doc, data);
            wsBroadcast(data.c_str());
          }

          else if(cmd == "setFInit"){
            if(doc[PSTR("fInit")] != nullptr){
              syncClientAttr(2);
              _setFinit(doc[PSTR("fInit")].as<bool>());
            }
            reboot(3);
          }

          else if(cmd == "setRTCUpdate"){
            if(doc[PSTR("ts")] != nullptr){
              rtcUpdate(doc[PSTR("ts")].as<unsigned long>());
            }
          }

          for (auto callback : _onWSEventCallbacks) { 
            callback(server, client, type, arg, data, len); // Call each callback
          }
        }
      }
      break;
    case WS_EVT_ERROR:
      {
        logger->warn(PSTR(__func__), PSTR("ws [%u] error\n"), client->id());
        return;
      }
      break;	
  }
}

void Udawa::addOnWsEvent(WsOnEventCallback callback) {
    _onWSEventCallbacks.push_back(callback);
}
#endif

void Udawa::_crashStateTruthKeeper(uint8_t direction){
  JsonDocument crashStateDoc;
  crashState.rtcp = millis();

  if(direction == 1 || direction == 3){
    _crashStateConfig.load(crashStateDoc);
    crashState.rtcp = crashStateDoc[PSTR("rtcp")];
    crashState.crashCnt = crashStateDoc[PSTR("crashCnt")];
    crashState.fSafeMode = crashStateDoc[PSTR("fSafeMode")];
    crashState.lastRecordedDatetime = crashStateDoc[PSTR("lastRecordedDatetime")];
  } 

   if(direction == 2 || direction == 3){
    crashStateDoc[PSTR("rtcp")] = crashState.rtcp;
    crashStateDoc[PSTR("crashCnt")] = crashState.crashCnt;
    crashStateDoc[PSTR("fSafeMode")] = crashState.fSafeMode;
    crashStateDoc[PSTR("lastRecordedDatetime")] = RTC.getEpoch();
    _crashStateConfig.save(crashStateDoc);
  }
}

#ifdef USE_IOT
void Udawa::_processThingsboardProvisionResponse(const JsonObjectConst &data){
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && !config.state.provSent && tb.connected()){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 1000 ) == pdTRUE )
    {
      constexpr char CREDENTIALS_TYPE[] PROGMEM = "credentialsType";
      constexpr char CREDENTIALS_VALUE[] PROGMEM = "credentialsValue";
      String _data;
      serializeJson(data, _data);
      logger->verbose(PSTR(__func__),PSTR("Received device provision response: %s\n"), _data.c_str());

      if (strncmp(data["status"], "SUCCESS", strlen("SUCCESS")) != 0) {
        logger->error(PSTR(__func__),PSTR("Provision response contains the error: (%s)\n"), data["errorMsg"].as<const char*>());
      }
      else
      {
        if (strncmp(data[CREDENTIALS_TYPE], PSTR("ACCESS_TOKEN"), strlen(PSTR("ACCESS_TOKEN"))) == 0) {
          strlcpy(config.state.accTkn, data[CREDENTIALS_VALUE].as<std::string>().c_str(), sizeof(config.state.accTkn));
          config.state.provSent = true;  
          config.save();
          logger->verbose(PSTR(__func__),PSTR("Access token provision response saved.\n"));
        }
        else if (strncmp(data[CREDENTIALS_TYPE], PSTR("MQTT_BASIC"), strlen(PSTR("MQTT_BASIC"))) == 0) {
          /*auto credentials_value = data[CREDENTIALS_VALUE].as<JsonObjectConst>();
          credentials.client_id = credentials_value[CLIENT_ID].as<std::string>();
          credentials.username = credentials_value[CLIENT_USERNAME].as<std::string>();
          credentials.password = credentials_value[CLIENT_PASSWORD].as<std::string>();*/
        }
        else {
          logger->warn(PSTR(__func__),PSTR("Unexpected provision credentialsType: (%s)\n"), data[CREDENTIALS_TYPE].as<const char*>());

        }
      }

      // Disconnect from the cloud client connected to the provision account, because it is no longer needed the device has been provisioned
      // and we can reconnect to the cloud with the newly generated credentials.
      if (tb.connected()) {
        tb.disconnect();
      }
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->error(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}


void Udawa::_pvTaskCodeThingsboard(void *pvParameters){
  #ifdef USE_IOT_SECURE
  _tcpClient.setCACert(CA_CERT);
  const char *ssl_protos[] = {PSTR("mqtt")};
  _tcpClient.setAlpnProtocols(ssl_protos);
  #endif
  while(true){
    if(!config.state.provSent){
      if (tb.connect(config.state.tbAddr, "provision", config.state.tbPort)) {
        const Provision_Callback provisionCallback(
            Access_Token(),
            [this](const JsonObjectConst &data) {
                this->_processThingsboardProvisionResponse(data);
            }
            ,
            config.state.provDK,
            config.state.provDS,
            config.state.name
        );
        if(tb.Provision_Request(provisionCallback))
        {
          logger->info(PSTR(__func__),PSTR("Connected to provisioning server: %s:%d. Sending provisioning response: DK: %s, DS: %s, Name: %s \n"),  
            config.state.tbAddr, config.state.tbPort, config.state.provDK, config.state.provDS, config.state.name);
        }
      }
      else
      {
        logger->warn(PSTR(__func__),PSTR("Failed to connect to provisioning server: %s:%d\n"),  config.state.tbAddr, config.state.tbPort);
      }
      unsigned long timer = millis();
      while(true){
        tb.loop();
        if(config.state.provSent || (millis() - timer) > 10000){break;}
        vTaskDelay((const TickType_t)10 / portTICK_PERIOD_MS);
      }
    }
    else{
      if(!tb.connected() && WiFi.isConnected())
      {
        for (auto callback : _onThingsboardDisconnectedCallbacks) { 
          callback(); // Call each callback
        }
        logger->warn(PSTR(__func__),PSTR("IoT disconnected!\n"));
        //onTbDisconnectedCb();
        logger->info(PSTR(__func__),PSTR("Connecting to broker %s:%d\n"), config.state.tbAddr, config.state.tbPort);
        uint8_t tbDisco = 0;
        while(!tb.connect(config.state.tbAddr, config.state.accTkn, config.state.tbPort, config.state.name)){  
          tbDisco++;
          logger->warn(PSTR(__func__),PSTR("Failed to connect to IoT Broker %s (%d)\n"), config.state.tbAddr, tbDisco);
          if(tbDisco >= 12){
            config.state.provSent = false;
            tbDisco = 0;
            break;
          }
          vTaskDelay((const TickType_t)5000 / portTICK_PERIOD_MS);
        }

        if(tb.connected()){
          if(!iotState.fSharedAttributesSubscribed){
            iotState.fSharedAttributesSubscribed = tb.Shared_Attributes_Subscribe(_thingsboardSharedAttributesUpdateCallback);
            if (iotState.fSharedAttributesSubscribed){
              logger->verbose(PSTR(__func__), PSTR("Thingsboard shared attributes update subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe Thingsboard shared attributes update.\n"));
            }
          }

          if(!iotState.fRebootRPCSubscribed){
            RPC_Callback rebootCallback("reboot", _thingsboardRPCRebootHandler);
            iotState.fRebootRPCSubscribed = tb.RPC_Subscribe(rebootCallback); // Pass the callback directly
            if(iotState.fRebootRPCSubscribed){
              logger->verbose(PSTR(__func__), PSTR("reboot RPC subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe reboot RPC.\n"));
            }
          }

          if(!iotState.fConfigSaveRPCSubscribed){
            RPC_Callback configSaveCallback("configSave", _thingsboardRPCConfigSaveHandler);
            iotState.fConfigSaveRPCSubscribed = tb.RPC_Subscribe(configSaveCallback); // Pass the callback directly
            if(iotState.fConfigSaveRPCSubscribed){
              logger->verbose(PSTR(__func__), PSTR("configSave RPC subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe configSave RPC.\n"));
            }
          }

          #ifdef USE_IOT_OTA
          iotState.fIoTCurrentFWSent = tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION) && tb.Firmware_Send_State(PSTR("UPDATED"));
          if(iotState.fIoTCurrentFWSent){
          //if(true){
            tb.Shared_Attributes_Request(_iotUpdaterFirmwareCheckCallback);
          }
          #endif

          for (auto callback : _onThingsboardConnectedCallbacks) { 
            callback(); // Call each callback
          }
          logger->info(PSTR(__func__),PSTR("IoT Connected!\n"));
        }
      }
      else{
        if(iotState.fIoTUpdateStarted){
          tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION) && tb.Firmware_Send_State(PSTR("UPDATED"));
          if (tb.Subscribe_Firmware_Update(_iotUpdaterOTACallback) && tb.Start_Firmware_Update(_iotUpdaterOTACallback)) {
              logger->debug(PSTR(__func__), PSTR("Firmware update started.\n"));
              // Firmware update started successfully
              // Continue with the update process
          } else {
              logger->error(PSTR(__func__), PSTR("Firmware update failed to start.\n"));
              // Handle the update failure
          }
          iotState.fIoTUpdateStarted = false;
        }
      }
    }

    tb.loop();
    vTaskDelay((const TickType_t) 1 / portTICK_PERIOD_MS);
  }
}

void Udawa::_pvTaskCodeThingsboardTaskWrapper(void* pvParameters) {  // Define as static
  Udawa* udawaInstance = static_cast<Udawa*>(pvParameters);
  udawaInstance->_pvTaskCodeThingsboard(pvParameters); 
}

void Udawa::_processThingsboardSharedAttributesUpdate(const JsonObjectConst &data){
  String _data;
  serializeJson(data, _data);
  logger->debug(PSTR(__func__), PSTR("%s\n"), _data.c_str());
  for (auto callback : _onThingsboardSharedAttributesReceivedCallbacks) { 
    callback(data); // Call each callback
  }
}

void Udawa::addOnThingsboardConnected(ThingsboardOnConnectedCallback callback){
  _onThingsboardConnectedCallbacks.push_back(callback);
}

void Udawa::addOnThingsboardDisconnected(ThingsboardOnDisconnectedCallback callback){
  _onThingsboardDisconnectedCallbacks.push_back(callback);
}

void Udawa::addOnThingsboardSharedAttributesReceived(ThingsboardOnSharedAttributesReceivedCallback callback) {
    _onThingsboardSharedAttributesReceivedCallbacks.push_back(callback);
}

void Udawa::_processThingsboardRPCReboot(const JsonVariantConst &data, JsonDocument &response) {
  if(data != nullptr && data.as<int>() >= 0){
    reboot(data.as<int>());
  }
  else{
    reboot(0);
  }
}

void Udawa::_processThingsboardRPCConfigSave(const JsonVariantConst &data, JsonDocument &response) {
  config.save();
}

void Udawa::_processIoTUpdaterFirmwareCheckAttributesRequest(const JsonObjectConst &data){
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && tb.connected()){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 5000 ) == pdTRUE )
    {
      if(data["fw_version"] != nullptr){
        logger->info(PSTR(__func__), PSTR("Firmware check local: %s vs cloud: %s\n"), CURRENT_FIRMWARE_VERSION, data["fw_version"].as<const char*>());
        if(strcmp(data["fw_version"].as<const char*>(), CURRENT_FIRMWARE_VERSION)){
          logger->debug(PSTR(__func__), PSTR("Updating firmware...\n"));
          iotState.fIoTUpdateStarted = true;
        }else{
          logger->debug(PSTR(__func__), PSTR("No need to update firmware.\n"));
          iotState.fIoTUpdateStarted = false;
        }
      }
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
}

void Udawa::_iotUpdaterUpdatedCallback(const bool& success){
  if(success){
    logger->info(PSTR(__func__), PSTR("IoT OTA Update done!\n"));
    reboot(10);
  }
  else{
    logger->warn(PSTR(__func__), PSTR("IoT OTA Update failed!\n"));
    reboot(10);
  }
}

void Udawa::_iotUpdaterProgressCallback(const size_t& currentChunk, const size_t& totalChuncks){
  if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 5000 ) == pdTRUE ) {
    logger->debug(PSTR(__func__), PSTR("IoT OTA Progress: %.2f%%\n"),  static_cast<float>(currentChunk * 100U) / totalChuncks);
    xSemaphoreGive( iotState.xSemaphoreThingsboard );   
  }
}

bool Udawa::iotSendAttributes(const char *buffer){
  bool res = false;
  int length = strlen(buffer);
  if (buffer[length - 1] != '}') {
      logger->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending attribute to broker: %s\n"), buffer);
      res = tb.sendAttributeJson(buffer);
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return res;
}

bool Udawa::iotSendAttributes(JsonDocument &doc){
  bool res = false;
  String buffer;
  serializeJson(doc, buffer);
  int length = strlen(buffer.c_str());
  if (buffer[length - 1] != '}') {
      logger->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending attribute to broker: %s\n"), buffer.c_str());
      res = tb.sendAttributeJson(buffer.c_str());
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }
  }
  return res;
}

bool Udawa::iotSendTelemetry(const char *buffer){
  bool res = false;
  int length = strlen(buffer);
  if (buffer[length - 1] != '}') {
      logger->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending telemetry to broker: %s\n"), buffer);
      res = tb.sendTelemetryJson(buffer); 
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }   
  }
  return res;
}

bool Udawa::iotSendTelemetry(JsonDocument &doc){
  bool res = false;
  String buffer;
  serializeJson(doc, buffer);
  int length = strlen(buffer.c_str());
  if (buffer[length - 1] != '}') {
      logger->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending telemetry to broker: %s\n"), buffer.c_str());
      res = tb.sendTelemetryJson(buffer.c_str()); 
      xSemaphoreGive( iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }   
  }
  return res;
}
#endif

void Udawa::rtcUpdate(long ts){
  #ifdef USE_HW_RTC
  crashState.fRTCHwDetected = false;
  if(!_hwRTC.begin()){
    logger->error(PSTR(__func__), PSTR("RTC module not found. Any function that requires precise timing will malfunction! \n"));
    logger->warn(PSTR(__func__), PSTR("Trying to recover last recorded time from flash file...! \n"));
    RTC.setTime(crashState.lastRecordedDatetime);
    logger->debug(PSTR(__func__), PSTR("Updated time via last recorded time from flash file: %s\n"), RTC.getDateTime().c_str());
  }
  else{
    crashState.fRTCHwDetected = true;
    _hwRTC.setSquareWave(SquareWaveDisable);
  }
  #endif
  if(ts == 0){
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org");
    timeClient.setTimeOffset(config.state.gmtOff);
    bool ntpSuccess = timeClient.update();
    if (ntpSuccess){
      long epochTime = timeClient.getEpochTime();
      RTC.setTime(epochTime);
      logger->debug(PSTR(__func__), PSTR("Updated time via NTP: %s GMT Offset:%d (%d) \n"), RTC.getDateTime().c_str(), config.state.gmtOff, config.state.gmtOff / 3600);
      #ifdef USE_HW_RTC
      if(crashState.fRTCHwDetected){
        logger->debug(PSTR(__func__), PSTR("Updating RTC HW from NTP...\n"));
        _hwRTC.setDateTime(RTC.getHour(), RTC.getMinute(), RTC.getSecond(), RTC.getDay(), RTC.getMonth()+1, RTC.getYear(), RTC.getDayofWeek());
        logger->debug(PSTR(__func__), PSTR("Updated RTC HW from NTP with epoch %d | H:I:S W D-M-Y. -> %d:%d:%d %d %d-%d-%d\n"), 
        _hwRTC.getEpoch(), RTC.getHour(), RTC.getMinute(), RTC.getSecond(), RTC.getDayofWeek(), RTC.getDay(), RTC.getMonth()+1, RTC.getYear());
      }
      #endif
    }else{
      #ifdef USE_HW_RTC
      if(crashState.fRTCHwDetected){
        logger->debug(PSTR(__func__), PSTR("Updating RTC from RTC HW with epoch %d.\n"), _hwRTC.getEpoch());
        RTC.setTime(_hwRTC.getEpoch());
        logger->debug(PSTR(__func__), PSTR("Updated time via RTC HW: %s GMT Offset:%d (%d) \n"), RTC.getDateTime().c_str(), config.state.gmtOff, config.state.gmtOff / 3600);
      }
      #endif
    }
  }else{
      RTC.setTime(ts);
      logger->debug(PSTR(__func__), PSTR("Updated time via timestamp: %s\n"), RTC.getDateTime().c_str());
  }
}

void Udawa::addOnSyncClientAttributesCallback(SyncClientAttributesCallback callback){
  _onSyncClientAttributesCallback.push_back(callback);
}

void Udawa::syncClientAttr(uint8_t direction){
  String ip = WiFi.localIP().toString();
  
  JsonDocument doc;
  char buffer[384];

  #ifdef USE_IOT
  if(tb.connected() && (direction == 0 || direction == 1) ){
    doc[PSTR("ipad")] = ip;
    doc[PSTR("compdate")] = COMPILED;
    doc[PSTR("fmTitle")] = CURRENT_FIRMWARE_TITLE;
    doc[PSTR("fmVersion")] = CURRENT_FIRMWARE_VERSION;
    doc[PSTR("stamac")] = WiFi.macAddress();
    doc[PSTR("apmac")] = WiFi.softAPmacAddress();
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
    doc[PSTR("flFree")] = ESP.getFreeSketchSpace();
    doc[PSTR("fwSize")] = ESP.getSketchSize();
    doc[PSTR("flSize")] = ESP.getFlashChipSize();
    doc[PSTR("dSize")] = (int)LittleFS.totalBytes(); 
    doc[PSTR("dUsed")] = (int)LittleFS.usedBytes();
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
    doc[PSTR("sdkVer")] = ESP.getSdkVersion();
    doc[PSTR("model")] = config.state.model;
    doc[PSTR("name")] = config.state.name;
    doc[PSTR("group")] = config.state.group;
    doc[PSTR("tbAddr")] = config.state.tbAddr;
    doc[PSTR("tbPort")] = config.state.tbPort;
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
    doc[PSTR("wssid")] = config.state.wssid;
    doc[PSTR("ap")] = WiFi.SSID();
    doc[PSTR("wpass")] = config.state.wpass;
    doc[PSTR("dssid")] = config.state.dssid;
    doc[PSTR("dpass")] = config.state.dpass;
    doc[PSTR("upass")] = config.state.upass;
    doc[PSTR("accTkn")] = config.state.accTkn;
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
    doc[PSTR("provDK")] = config.state.provDK;
    doc[PSTR("provDS")] = config.state.provDS;
    doc[PSTR("logLev")] = config.state.logLev;
    doc[PSTR("gmtOff")] = config.state.gmtOff;
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
    doc[PSTR("fWOTA")] = (int)config.state.fWOTA;
    doc[PSTR("fWeb")] = (int)config.state.fWeb;
    doc[PSTR("hname")] = config.state.hname;
    doc[PSTR("logIP")] = config.state.logIP;
    doc[PSTR("logPort")] = config.state.logPort;
    doc[PSTR("htU")] = config.state.htU;
    doc[PSTR("htP")] = config.state.htP;
    serializeJson(doc, buffer);
    iotSendAttributes(buffer);
    doc.clear();
  }
  #endif

  #ifdef USE_LOCAL_WEB_INTERFACE
  if((direction == 0 || direction == 2)){
    JsonObject attr = doc["attr"].to<JsonObject>(); 
    attr[PSTR("ipad")] = ip.c_str();
    attr[PSTR("compdate")] = COMPILED;
    attr[PSTR("fmTitle")] = CURRENT_FIRMWARE_TITLE;
    attr[PSTR("fmVersion")] = CURRENT_FIRMWARE_VERSION;
    attr[PSTR("stamac")] = WiFi.macAddress();
    attr[PSTR("apmac")] = WiFi.softAPmacAddress();
    attr[PSTR("flFree")] = ESP.getFreeSketchSpace();
    attr[PSTR("fwSize")] = ESP.getSketchSize();
    attr[PSTR("flSize")] = ESP.getFlashChipSize();
    attr[PSTR("dSize")] = (int)LittleFS.totalBytes(); 
    attr[PSTR("dUsed")] = (int)LittleFS.usedBytes();
    attr[PSTR("sdkVer")] = ESP.getSdkVersion();
    serializeJson(doc, buffer);
    wsBroadcast(buffer);
    doc.clear();
    doc[PSTR("cmd")] = PSTR("setConfig");
    JsonObject cfg = doc[PSTR("cfg")].to<JsonObject>(); 
    cfg[PSTR("name")] = config.state.name;
    cfg[PSTR("model")] = config.state.model;
    cfg[PSTR("group")] = config.state.group;
    cfg[PSTR("gmtOff")] = config.state.gmtOff;
    cfg[PSTR("hname")] = config.state.hname;
    cfg[PSTR("htP")] = config.state.htP;
    cfg[PSTR("wssid")] = config.state.wssid;
    cfg[PSTR("wpass")] = config.state.wpass;
    cfg[PSTR("fInit")] = config.state.fInit;
    serializeJson(doc, buffer);
    wsBroadcast(buffer);
  }
  #endif

  for (auto callback : _onSyncClientAttributesCallback) { 
    callback(direction); // Call each callback
  }
}

void Udawa::I2CScanner(JsonDocument &doc){
  JsonArray i2c = doc[PSTR("i2c")].to<JsonArray>();
  for (uint8_t i = 0; i < 127; i++) {
    Wire.beginTransmission(i);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      logger->debug(PSTR(__func__), PSTR("I2C device found at address 0x%02X\n"), i);
      i2c.add(i);
    }
  }
}

void Udawa::I2CScanner(){
  for (uint8_t i = 0; i < 127; i++) {
    Wire.beginTransmission(i);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      logger->debug(PSTR(__func__), PSTR("I2C device found at address 0x%02X\n"), i);
    }
  }
}