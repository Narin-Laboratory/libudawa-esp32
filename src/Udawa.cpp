#include "Udawa.h"

Udawa* Udawa::instance = nullptr;

Udawa* Udawa::getInstance() {
  if (instance == nullptr) {
    instance = new Udawa();
  }
  return instance;
}

Udawa::Udawa() : config(PSTR("/config.json")), _crashStateConfig(PSTR("/crash.json"))
  ,RTC(0)
  #ifdef USE_LOCAL_WEB_INTERFACE
  ,http(80) 
  ,ws(PSTR("/ws"))
  #endif
  {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);

    #ifdef USE_LOCAL_WEB_INTERFACE
    xSemaphoreWSBroadcast = NULL;
    if(xSemaphoreWSBroadcast == NULL){xSemaphoreWSBroadcast = xSemaphoreCreateMutex();}
    #endif
}

void Udawa::begin(){
    _xQueueAlarm = xQueueCreate( 10, sizeof( struct AlarmMessage ) );
    logger->debug(PSTR(__func__), PSTR("Initializing LittleFS: %d\n"), config.begin());
    config.load();
    
    logger->setLogLevel((LogLevel)config.state.logLev);
    setAlarm(0, 0, 3, 50);

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
        if(crashState.crashCnt >= MAX_CRASH_COUNTER){
            crashState.fSafeMode = true;
            logger->warn(PSTR(__func__), PSTR("** SAFEMODE ACTIVATED **\n"));
        }
    }
    logger->debug(PSTR(__func__), PSTR("Runtime Counter: %d, Crash Counter: %d, Safemode Status: %s\n"), crashState.rtcp, crashState.crashCnt, crashState.fSafeMode ? PSTR("ENABLED") : PSTR("DISABLED"));

    if (!crashState.fSafeMode){
      logger->info(PSTR(__func__), PSTR("Hardware ID: %s\n"), (String(config.state.model) + String(config.state.hwid)).c_str() );

      #ifdef USE_WIFI_LOGGER
      wiFiLogger->setConfig(config.state.logIP, config.state.logPort, WIFI_LOGGER_BUFFER_SIZE);
      #endif

      if(_xHandleAlarm == NULL){
        _xReturnedAlarm = xTaskCreatePinnedToCore(_alarmTaskRoutine, PSTR("alarmTaskRoutine"), ALARM_STACKSIZE, this, 1, &_xHandleAlarm, 1);
        if(_xReturnedAlarm == pdPASS){
          logger->warn(PSTR(__func__), PSTR("Task alarmTaskRoutine has been created.\n"));
        }
      }

      //Server_Side_RPC<> rpc;
      /*const std::array<IAPI_Implementation*, 1U> apis = {
          &rpc
      };*/

      

      /*WiFiClient _tcpClient;
      Arduino_MQTT_Client _mqttClient(_tcpClient);
      ThingsBoardSized<10, 10, UdawaThingsboardLogger> tb(_mqttClient, 1024, 1024, 1024, 2048);
      tb.connect("prita.undiknas.ac.id", "TOKEN", 8883);
      const std::array<RPC_Callback, 1> callbacks = {
        // Requires additional memory in the JsonDocument for the JsonDocument that will be copied into the response
        RPC_Callback{ "test",           nullptr }
      };
      rpc.RPC_Subscribe(callbacks.cbegin(), callbacks.cend());
      tb.Subscribe_API_Implementation(rpc);*/
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
      logger->verbose(PSTR(__func__), PSTR("Crash state saved.\n"));
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

    if(crashState.fFSDownloading){
      if(WiFi.status() == WL_CONNECTED){
        logger->warn(PSTR(__func__), PSTR("Filesystem update is started.\n"));
        crashState.fFSDownloading = false;
        FSDownloader();
      }
      else{
        logger->warn(PSTR(__func__), PSTR("Filesystem update is postponed until WiFi is available.\n"));
      }
    }

    if(crashState.fStartServices){
      crashState.fStartServices = false;
      _startServices();
    }

    if(crashState.fStopServices){
      crashState.fStopServices = false;
      _stopServices();
    }

    if(crashState.fDoInit){
      crashState.fDoInit = false;
      _doInit();
    }
}

void Udawa::_setLEDBuzzer(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay){
  uint8_t r, g, b;
  switch (color)
  {
  //Auto by network
  case 0:
    if(false){
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
          alarm[PSTR("time")] = self->RTC.getDateTime();

          #ifdef USE_LOCAL_WEB_INTERFACE
          self->wsBroadcast(doc);
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

void Udawa::_setFInit(bool fInit){
  config.state.fInit = fInit;
  config.save();

  #ifdef USE_LOCAL_WEB_INTERFACE
    if(config.state.fWeb && !crashState.fSafeMode){
      JsonDocument doc;
      doc[PSTR("setFinishedSetup")][PSTR("fInit")] = config.state.fInit;
      wsBroadcast(doc);
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
  http.serveStatic("/css/index.css", LittleFS, "/ui/css/index.css");
  http.serveStatic("/assets/bundle.js", LittleFS, "/ui/assets/bundle.js");
  

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
        logger->debug(PSTR(__func__), PSTR("mDNS responder started at %s\n"), config.state.hname);
    }

    MDNS.addService("http", "tcp", 80);

    #ifdef USE_LOCAL_WEB_INTERFACE
    if(config.state.fWeb && !crashState.fSafeMode){
      logger->debug(PSTR(__func__), PSTR("Starting Web Service...\n"));
      http.serveStatic("/", LittleFS, "/ui").setDefaultFile("index.html");
      http.serveStatic("/css/pico.blue.min.css", LittleFS, "/ui/css/pico.blue.min.css");
      http.serveStatic("/css/index.css", LittleFS, "/ui/css/index.css");
      http.serveStatic("/assets/bundle.js", LittleFS, "/ui/assets/bundle.js");

      ws.onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
          this->_onWsEvent(server, client, type, arg, data, len);
      });

      http.addHandler(&ws);
      http.begin();
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
  crashState.fDoInit = true;
  setAlarm(0, 0, 3, 50);
}

void Udawa::_onWiFiGotIP(){
  crashState.fStartServices = true;
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
        //logger->verbose(PSTR(__func__), PSTR("Broadcasting message: %s\n"), buffer);
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
        //logger->verbose(PSTR(__func__), PSTR("Broadcasting message: %s\n"), buffer.c_str());
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
            if(doc[PSTR("auth")][PSTR("salt")] == nullptr || doc[PSTR("auth")][PSTR("hash")] == nullptr){
              //client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
              //_wsClientAuthAttemptTimestamps[clientIP] = currentTime;
              return;
            }

            String clientAuth = doc[PSTR("auth")][PSTR("hash")].as<String>();
            String clientSalt = doc[PSTR("auth")][PSTR("salt")].as<String>();
            //logger->debug(PSTR(__func__), PSTR("\n\tserver: %s\n\tclient: %s\n\tkey: %s\n\tsalt: %s\n"), _auth.c_str(), auth.c_str(), config.state.htP, salt.c_str());
            
            if (_wsClientSalts[client->id()] == clientSalt) {
                // Compute expected HMAC with stored salt
                String expectedAuth = hmacSha256(config.state.htP, _wsClientSalts[client->id()]);

                // Check if the HMACs match
                //logger->verbose(PSTR(__func__), PSTR("\nhtP:\t%s \n\nclientAuth:\t%s\n\nexpectedAuth:\t%s\n"), config.state.htP, clientAuth.c_str(), expectedAuth.c_str());
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

        if (doc[PSTR("setConfig")].is<JsonObject>()) {
          if (doc[PSTR("setConfig")][PSTR("cfg")].is<JsonObject>()) {
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wssid")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wssid")].as<const char*>()) > 0) {
            strlcpy(config.state.wssid, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wssid")].as<const char*>(), sizeof(config.state.wssid));
            logger->debug(PSTR(__func__), PSTR("wssid: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wssid")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wpass")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wpass")].as<const char*>()) > 0) {
            strlcpy(config.state.wpass, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wpass")].as<const char*>(), sizeof(config.state.wpass));
            logger->debug(PSTR(__func__), PSTR("wpass: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("wpass")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("gmtOff")].is<int>()) {
            config.state.gmtOff = doc[PSTR("setConfig")][PSTR("cfg")][PSTR("gmtOff")].as<int>();
            logger->debug(PSTR(__func__), PSTR("gmtOff: %d\n"), config.state.gmtOff);  // Display as integer
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("group")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("group")].as<const char*>()) > 0) {
            strlcpy(config.state.group, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("group")].as<const char*>(), sizeof(config.state.group));
            logger->debug(PSTR(__func__), PSTR("group: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("group")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("name")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("name")].as<const char*>()) > 0) {
            strlcpy(config.state.name, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("name")].as<const char*>(), sizeof(config.state.name));
            logger->debug(PSTR(__func__), PSTR("name: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("name")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("hname")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("hname")].as<const char*>()) > 0) {
            strlcpy(config.state.hname, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("hname")].as<const char*>(), sizeof(config.state.hname));
            logger->debug(PSTR(__func__), PSTR("hname: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("hname")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("htP")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("htP")].as<const char*>()) > 0) {
            strlcpy(config.state.htP, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("htP")].as<const char*>(), sizeof(config.state.htP));
            logger->debug(PSTR(__func__), PSTR("htP: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("htP")].as<const char*>());
            }
            if (doc[PSTR("setConfig")][PSTR("cfg")][PSTR("binURL")].is<const char*>() && strlen(doc[PSTR("setConfig")][PSTR("cfg")][PSTR("binURL")].as<const char*>()) > 0) {
            strlcpy(config.state.binURL, doc[PSTR("setConfig")][PSTR("cfg")][PSTR("binURL")].as<const char*>(), sizeof(config.state.binURL));
            logger->debug(PSTR(__func__), PSTR("binURL: %s\n"), doc[PSTR("setConfig")][PSTR("cfg")][PSTR("binURL")].as<const char*>());
            }
          }
          config.save();
          }

          else if(doc[PSTR("getConfig")].is<const char*>()){
            syncClientAttr(2);
          }

          else if(doc[PSTR("getAvailableWiFi")].is<const char*>()){
            JsonDocument doc;
            JsonDocument WiFiList;
            File file = LittleFS.open("/WiFiList.json", FILE_READ);
            deserializeJson(WiFiList, file);
            file.close();
            doc[PSTR("WiFiList")] = WiFiList;
            String data;
            serializeJson(doc, data);
            wsBroadcast(data.c_str());
          }

          else if(doc[PSTR("setFInit")].is<JsonObject>()){
            if(doc[PSTR("setFInit")][PSTR("fInit")].is<bool>()){
              syncClientAttr(2);
              _setFInit(doc[PSTR("setFInit")][PSTR("fInit")].as<bool>());
            }
            reboot(3);
          }

          else if(doc[PSTR("setRTCUpdate")].is<JsonObject>()){
            if(doc[PSTR("setRTCUpdate")][PSTR("ts")].is<unsigned long>()){
              rtcUpdate(doc[PSTR("setRTCUpdate")][PSTR("ts")].as<unsigned long>());
            }
          }

          else if(doc[PSTR("reboot")].is<int>()){
            reboot(doc[PSTR("reboot")].as<int>());
          }

          else if(doc[PSTR("FSUpdate")].is<bool>()){
            crashState.fFSDownloading = true;
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

void Udawa::addOnFSDownloadedCallback(FSDownloadedCallback callback){
  _onFSDownloadedCallback.push_back(callback);
}

void Udawa::syncClientAttr(uint8_t direction){
  String ip = WiFi.localIP().toString();
  
  JsonDocument doc;
  char buffer[512];

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
    cfg[PSTR("binURL")] = config.state.binURL;
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

void Udawa::FSDownloader(){
  HTTPClient http;

  logger->info(PSTR(__func__), PSTR("Downloading SPIFFS: %s.\n"), config.state.binURL);

  http.begin( config.state.binURL );

  const char* get_headers[] = { "Content-Length", "Content-type", "Accept-Ranges" };
  http.collectHeaders( get_headers, sizeof(get_headers)/sizeof(const char*) );

  int64_t updateSize = 0;
  int httpCode = http.GET();
  String contentType;

  if( httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY ) {
      updateSize = http.getSize();
      contentType = http.header( "Content-type" );
      String acceptRange = http.header( "Accept-Ranges" );
      if( acceptRange == "bytes" ) {
          logger->info(PSTR(__func__), PSTR("This server supports resume!\n"));
      } else {
          logger->info(PSTR(__func__), PSTR("This server dose not supports resume!\n"));
      }
  } else {
      logger->info(PSTR(__func__), PSTR("Server responded with HTTP Status %s.\n"), String(httpCode).c_str());
      reboot(300);
      return;
  }

  // TODO: Not all streams respond with a content length.
  // TODO: Set updateSize to UPDATE_SIZE_UNKNOWN when content type is valid.

  // check updateSize and content type
  if( updateSize<=0 ) {
      logger->info(PSTR(__func__), PSTR("Response is empty! updateSize: %d, contentType: %s\n"), (int)updateSize, contentType.c_str());
      reboot(3);
      return;
  }

  logger->info(PSTR(__func__), PSTR("updateSize: %d, contentType: %s\n"), (int)updateSize, contentType.c_str());

  Stream* stream = http.getStreamPtr();
  if( updateSize<=0 || stream == nullptr ) {
      logger->warn(PSTR(__func__), PSTR("HTTP Error.\n"));
      reboot(3);
      return;
  }

  // some network streams (e.g. Ethernet) can be laggy and need to 'breathe'
  if( !stream->available() ) {
      uint32_t timeout = millis() + 3000;
      while( stream->available() ) {
          if( millis()>timeout ) {
              logger->warn(PSTR(__func__), PSTR("Stream timed out!\n"));
              reboot(3);
              return;
          }
          vTaskDelay((const TickType_t)10 / portTICK_PERIOD_MS);
      }
  }

  // If using compression, the size is implicitely unknown
  size_t fwsize = updateSize;       // fw_size is unknown if we have a compressed image

  bool canBegin = Update.begin(updateSize, U_SPIFFS);

  if( !canBegin ) {
      logger->warn(PSTR(__func__), PSTR("Not enough space to begin OTA, partition size mismatch?\n"));
      Update.abort();
      reboot(3);
      return;
  }

  Update.onProgress( [](size_t progress, size_t size) {
    Serial.printf("LittleFS Updater: %d/%d\n", (int)progress, (int)size);
  });

  logger->info(PSTR(__func__), PSTR("Begin LittleFS OTA. This may take 2 - 5 mins to complete. Things might be quiet for a while.. Patience!\n"));
  // Some activity may appear in the Serial monitor during the update (depends on Update.onProgress)
  size_t written = 0;
  while (written < updateSize) {
      size_t bytesWritten = Update.writeStream(*stream);
      if (bytesWritten == 0) {
          logger->warn(PSTR(__func__), PSTR("Stream write error.\n"));
          Update.abort();
          config.save();
          for (auto callback : _onFSDownloadedCallback) { 
            callback(); // Call each callback
          }
          reboot(3);
          return;
      }
      written += bytesWritten;
      logger->info(PSTR(__func__), PSTR("Written : %d / %d.\n"), (int)written, (int)updateSize);
  }

  if (written == updateSize) {
      logger->info(PSTR(__func__), PSTR("Written : %d successfully. \n"), (int)written);
  } else {
      logger->warn(PSTR(__func__), PSTR("Written only : %d / %d. Premature end of stream?\n"), (int)written, (int)updateSize);
      Update.abort();
      config.save();
      for (auto callback : _onFSDownloadedCallback) { 
        callback(); // Call each callback
      }
      reboot(3);
      return;
  }

  if (!Update.end()) {
      logger->warn(PSTR(__func__), PSTR("An Update Error Occurred: %d\n"), Update.getError());
      config.save();
      for (auto callback : _onFSDownloadedCallback) { 
        callback(); // Call each callback
      }
      reboot(3);
      return;
  }
  if (Update.isFinished()) {
      logger->info(PSTR(__func__), PSTR("Update completed successfully.\n"));
      config.save();
      for (auto callback : _onFSDownloadedCallback) { 
        callback(); // Call each callback
      }
      delay(1000); // Ensure configuration is saved before reboot
      reboot(3);
  } else {
      config.save();
      for (auto callback : _onFSDownloadedCallback) { 
        callback(); // Call each callback
      }
      logger->warn(PSTR(__func__), PSTR("Update not finished! Something went wrong!\n"));
      reboot(3);
  }
  reboot(3);
}