#include "Udawa.h"

#ifdef USE_LOCAL_WEB_INTERFACE
Udawa::Udawa() : config(PSTR("/config.json")), http(80), ws(PSTR("/ws")), _crashStateConfig(PSTR("/crash.json")),
  _mqttClient(_tcpClient), _tb(_mqttClient, IOT_MAX_MESSAGE_SIZE)  {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);
    if(_iotState.xSemaphoreThingsboard == NULL){_iotState.xSemaphoreThingsboard = xSemaphoreCreateMutex();}
}
#else
Udawa::Udawa() : config("/config.json") {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);
}
#endif

void Udawa::begin(){
    logger->debug(PSTR(__func__), PSTR("Initializing LittleFS: %d\n"), config.begin());
    config.load();
    
    logger->setLogLevel((LogLevel)config.state.logLev);

    wiFiHelper.begin(config.state.wssid, config.state.wpass, config.state.dssid, config.state.dpass, config.state.model);
    wiFiHelper.addOnConnectedCallback(std::bind(&Udawa::_onWiFiConnected, this));
    wiFiHelper.addOnGotIPCallback(std::bind(&Udawa::_onWiFiGotIP, this));
    wiFiHelper.addOnDisconnectedCallback(std::bind(&Udawa::_onWiFiDisconnected, this));

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

    
}

void Udawa::_onWiFiConnected(){
    
}

void Udawa::_onWiFiDisconnected(){

}

void Udawa::_onWiFiGotIP(){
    #ifdef USE_WIFI_OTA
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
    #endif

    if (!MDNS.begin(config.state.hname)) {
        logger->error(PSTR(__func__), PSTR("Error setting up MDNS responder!\n"));
    }
    else{
        logger->debug(PSTR(__func__), PSTR("mDNS responder started at %s.\n"), config.state.hname);
    }

    MDNS.addService("http", "tcp", 80);

    #ifdef USE_LOCAL_WEB_INTERFACE
    http.serveStatic("/", LittleFS, "/www").setDefaultFile("index.html");

    ws.onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
        this->_onWsEvent(server, client, type, arg, data, len);
    });

    http.addHandler(&ws);
    http.begin();
    #endif

    #ifdef USE_IOT
    if(config.state.fIoT && _iotState.xHandleIoT == NULL && !crashState.fSafeMode){
      _iotState.xReturnedIoT = xTaskCreatePinnedToCore(_pvTaskCodeThingsboardTaskWrapper, PSTR("Thingsboard"), IOT_STACKSIZE_TB, this, 1, &_iotState.xHandleIoT, 1);
      if(_iotState.xReturnedIoT == pdPASS){
        logger->warn(PSTR(__func__), PSTR("Task Thingsboard has been created.\n"));
      }
    }
    #endif
}

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

#ifdef USE_LOCAL_WEB_INTERFACE
void Udawa::_onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  IPAddress clientIP = client->remoteIP();
  switch(type) {
    case WS_EVT_DISCONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("Client disconnected [%s]\n"), clientIP.toString().c_str());
        // Remove client from maps
        _clientAuthenticationStatus.erase(client->id());
        _clientAuthAttemptTimestamps.erase(clientIP);
        logger->debug(PSTR(__func__), PSTR("ws [%u] disconnect.\n"), client->id());        
      }
      break;
    case WS_EVT_CONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("New client arrived [%s]\n"), clientIP.toString().c_str());
        // Initialize client as unauthenticated
        _clientAuthenticationStatus[client->id()] = false;
        // Initialize timestamp for rate limiting
        _clientAuthAttemptTimestamps[clientIP] = millis();
      }
      break;
    case WS_EVT_DATA:
      {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data);
        if(err != DeserializationError::Ok){
          logger->error(PSTR(__func__), PSTR("Failed to parse JSON.\n"));
          return;
        }
        
        // If client is not authenticated, check credentials
        if(!_clientAuthenticationStatus[client->id()]) {
          unsigned long currentTime = millis();
          unsigned long lastAttemptTime = _clientAuthAttemptTimestamps[clientIP];

          if (currentTime - lastAttemptTime < 1000) {
            // Too many attempts in short time, block this IP for blockInterval
            //_clientAuthAttemptTimestamps[clientIP] = currentTime + WS_BLOCKED_DURATION - WS_RATE_LIMIT_INTERVAL;
            logger->verbose(PSTR(__func__), PSTR("Too many authentication attempts. Blocking for %d seconds. Rate limit %d.\n"), WS_BLOCKED_DURATION / 1000, WS_RATE_LIMIT_INTERVAL);
            client->close();
            return;
          }

          if (err != DeserializationError::Ok) {
            client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
            _clientAuthAttemptTimestamps[clientIP] = currentTime;
            return;
          }
          else{
            if(doc["salt"] == nullptr || doc["auth"] == nullptr){
              client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
              _clientAuthAttemptTimestamps[clientIP] = currentTime;
              return;
            }

            String salt = doc["salt"];
            String auth = doc["auth"];
          
            if (hmacSha256(config.state.htP, salt) == auth) {
              // Client authenticated successfully, update the status in the map
              _clientAuthenticationStatus[client->id()] = true;
              client->printf(PSTR("{\"status\": {\"code\": 200, \"msg\": \"Authorized.\", \"model\": \"%s\"}}"), config.state.model);
              logger->debug(PSTR(__func__), PSTR("ws [%u] authenticated.\n"), client->id());
            } else {
              // Unauthorized, you can choose to disconnect the client
              client->text(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Unauthorized.\"}}"));
              client->close();
            }

            // Update timestamp for rate limiting
            _clientAuthAttemptTimestamps[clientIP] = currentTime;
            return;
          }
        }
        else {
          // The client is already authenticated, you can process the received data
          //...
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

  for (auto callback : _onWSEventCallbacks) { 
    callback(server, client, type, arg, data, len); // Call each callback
  }
}

void Udawa::addOnWsEvent(WsOnEventCallback callback) {
    _onWSEventCallbacks.push_back(callback);
}
#endif

String Udawa::hmacSha256(const String& message, const String& salt) {
// Convert input strings to UTF-8
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // HMAC mode
  mbedtls_sha256_update(&ctx, (const unsigned char*)config.state.htP, strlen(config.state.htP));
  mbedtls_sha256_update(&ctx, (const unsigned char*)salt.c_str(), salt.length());
  mbedtls_sha256_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  unsigned char hash[32];
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  return base64::encode(hash, 32);
}

void Udawa::_crashStateTruthKeeper(uint8_t direction){
  JsonDocument crashStateDoc;
  crashState.rtcp = millis();

  if(direction == 1 || direction == 3){
    _crashStateConfig.load(crashStateDoc);
    crashState.rtcp = crashStateDoc[PSTR("rtcp")];
    crashState.crashCnt = crashStateDoc[PSTR("crashCnt")];
    crashState.fSafeMode = crashStateDoc[PSTR("fSafeMode")];
  } 

   if(direction == 2 || direction == 3){
    crashStateDoc[PSTR("rtcp")] = crashState.rtcp;
    crashStateDoc[PSTR("crashCnt")] = crashState.crashCnt;
    crashStateDoc[PSTR("fSafeMode")] = crashState.fSafeMode;
    
    _crashStateConfig.save(crashStateDoc);
  }
}


void Udawa::_processThingsboardProvisionResponse(const Provision_Data &data){
  if( _iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && !config.state.provSent && _tb.connected()){
    if( xSemaphoreTake( _iotState.xSemaphoreThingsboard, ( TickType_t ) 1000 ) == pdTRUE )
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
      if (_tb.connected()) {
        _tb.disconnect();
      }
      xSemaphoreGive( _iotState.xSemaphoreThingsboard );
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
      if (_tb.connect(config.state.tbAddr, "provision", config.state.tbPort)) {
        const Provision_Callback provisionCallback(
            Access_Token(),
            [this](const Provision_Data &data) {
                this->_processThingsboardProvisionResponse(data);
            }
            ,
            config.state.provDK,
            config.state.provDS,
            config.state.name
        );
        if(_tb.Provision_Request(provisionCallback))
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
        _tb.loop();
        if(config.state.provSent || (millis() - timer) > 10000){break;}
        vTaskDelay((const TickType_t)10 / portTICK_PERIOD_MS);
      }
    }
    else{
      if(!_tb.connected() && WiFi.isConnected())
      {
        logger->warn(PSTR(__func__),PSTR("IoT disconnected!\n"));
        //onTbDisconnectedCb();
        logger->info(PSTR(__func__),PSTR("Connecting to broker %s:%d\n"), config.state.tbAddr, config.state.tbPort);
        uint8_t tbDisco = 0;
        while(!_tb.connect(config.state.tbAddr, config.state.accTkn, config.state.tbPort, config.state.name)){  
          tbDisco++;
          logger->warn(PSTR(__func__),PSTR("Failed to connect to IoT Broker %s (%d)\n"), config.state.tbAddr, tbDisco);
          if(tbDisco >= 12){
            config.state.provSent = false;
            tbDisco = 0;
            break;
          }
          vTaskDelay((const TickType_t)5000 / portTICK_PERIOD_MS);
        }

        if(_tb.connected()){
          /*bool tbSharedUpdate_status = tb.Shared_Attributes_Subscribe(tbSharedAttrUpdateCb);
          bool tbClientRPC_status = tb.RPC_Subscribe(clientRPCCallbacks.cbegin(), clientRPCCallbacks.cend());
          tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION); 
          tb.Firmware_Send_State(PSTR("updated"));
          tb.Shared_Attributes_Request(fwCheckCb);

          LAST_TB_CONNECTED = millis();

          setAlarm(0, 0, 3, 50);*/
          logger->info(PSTR(__func__),PSTR("IoT Connected!\n"));
        }
      }
    }

    _tb.loop();
    vTaskDelay((const TickType_t) 10 / portTICK_PERIOD_MS);
  }
}

void Udawa::_pvTaskCodeThingsboardTaskWrapper(void* pvParameters) {  // Define as static
    Udawa* udawaInstance = static_cast<Udawa*>(pvParameters);
    udawaInstance->_pvTaskCodeThingsboard(pvParameters); 
}