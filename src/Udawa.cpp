#include "Udawa.h"

Udawa::Udawa() : config(PSTR("/config.json")), _crashStateConfig(PSTR("/crash.json"))
  #ifdef USE_LOCAL_WEB_INTERFACE
  ,http(80), 
  ws(PSTR("/ws"))
  #endif
  #ifdef USE_IOT
  ,_mqttClient(_tcpClient), 
  _tb(_mqttClient, IOT_MAX_MESSAGE_SIZE)
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
    #ifdef USE_IOT
    _tb.setBufferSize(IOT_BUFFER_SIZE);
    if(_iotState.xSemaphoreThingsboard == NULL){_iotState.xSemaphoreThingsboard = xSemaphoreCreateMutex();}
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
    logger->debug(PSTR(__func__), PSTR("Initializing LittleFS: %d\n"), config.begin());
    config.load();
    
    logger->setLogLevel((LogLevel)config.state.logLev);
    #ifdef USE_WIFI_LOGGER
    wiFiLogger->setConfig(config.state.logIP, config.state.logPort, WIFI_LOGGER_BUFFER_SIZE);
    #endif

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

void Udawa::_onWiFiConnected(){
    
}

void Udawa::_onWiFiDisconnected(){

}

void Udawa::_onWiFiGotIP(){
    #ifdef USE_WIFI_LOGGER
    logger->addLogger(wiFiLogger);
    #endif
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
    if(config.state.fIoT && _iotState.xHandleIoT == NULL && !crashState.fSafeMode){
      _iotState.xReturnedIoT = xTaskCreatePinnedToCore(_pvTaskCodeThingsboardTaskWrapper, PSTR("Thingsboard"), IOT_STACKSIZE_TB, this, 1, &_iotState.xHandleIoT, 1);
      if(_iotState.xReturnedIoT == pdPASS){
        logger->warn(PSTR(__func__), PSTR("Task Thingsboard has been created.\n"));
      }
    }
    #endif
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
void Udawa::_onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  IPAddress clientIP = client->remoteIP();
  switch(type) {
    case WS_EVT_DISCONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("Client disconnected.\n"));
        // Remove client from maps
        _wsClientAuthenticationStatus.erase(client->id());
        _wsClientAuthAttemptTimestamps.erase(clientIP);
        logger->debug(PSTR(__func__), PSTR("ws [%u] disconnect.\n"), client->id());        
      }
      break;
    case WS_EVT_CONNECT:
      {
        logger->verbose(PSTR(__func__), PSTR("New client arrived [%s]\n"), clientIP.toString().c_str());
        // Initialize client as unauthenticated
        _wsClientAuthenticationStatus[client->id()] = false;
        // Initialize timestamp for rate limiting
        _wsClientAuthAttemptTimestamps[clientIP] = millis();
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
        if(!_wsClientAuthenticationStatus[client->id()]) {
          unsigned long currentTime = millis();
          unsigned long lastAttemptTime = _wsClientAuthAttemptTimestamps[clientIP];

          if (currentTime - lastAttemptTime < 1000) {
            // Too many attempts in short time, block this IP for blockInterval
            //_wsClientAuthAttemptTimestamps[clientIP] = currentTime + WS_BLOCKED_DURATION - WS_RATE_LIMIT_INTERVAL;
            logger->verbose(PSTR(__func__), PSTR("Too many authentication attempts. Blocking for %d seconds. Rate limit %d.\n"), WS_BLOCKED_DURATION / 1000, WS_RATE_LIMIT_INTERVAL);
            client->close();
            return;
          }

          if (err != DeserializationError::Ok) {
            client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
            _wsClientAuthAttemptTimestamps[clientIP] = currentTime;
            return;
          }
          else{
            if(doc["salt"] == nullptr || doc["auth"] == nullptr){
              client->printf(PSTR("{\"status\": {\"code\": 400, \"msg\": \"Bad request.\"}}"));
              _wsClientAuthAttemptTimestamps[clientIP] = currentTime;
              return;
            }

            String salt = doc["salt"];
            String auth = doc["auth"];
            String _auth = hmacSha256(String(config.state.htP), salt);
            //logger->debug(PSTR(__func__), PSTR("\n\t_auth: %s\n\tauth: %s\n\tkey: %s\n\tsalt: %s\n"), _auth.c_str(), auth.c_str(), config.state.htP, salt.c_str());
            if (_auth == auth) {
              // Client authenticated successfully, update the status in the map
              _wsClientAuthenticationStatus[client->id()] = true;
              client->printf(PSTR("{\"status\": {\"code\": 200, \"msg\": \"Authorized.\", \"model\": \"%s\"}}"), config.state.model);
              logger->debug(PSTR(__func__), PSTR("ws [%u] authenticated.\n"), client->id());
            } else {
              // Unauthorized, you can choose to disconnect the client
              client->text(PSTR("{\"status\": {\"code\": 401, \"msg\": \"Unauthorized.\"}}"));
              client->close();
            }

            // Update timestamp for rate limiting
            _wsClientAuthAttemptTimestamps[clientIP] = currentTime;
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

String Udawa::hmacSha256(String htP, String salt) {
// Convert input strings to UTF-8
  unsigned char hash[32];
  const byte* key = (const byte*)htP.c_str();
  size_t keyLength = htP.length();
  const byte* message = (const byte*)salt.c_str();
  size_t messageLength = salt.length();
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, key, keyLength);
  mbedtls_md_hmac_update(&ctx, message, messageLength);
  mbedtls_md_hmac_finish(&ctx, hash);
  mbedtls_md_free(&ctx); 
  return base64::encode(hash, 32);
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
  } 

   if(direction == 2 || direction == 3){
    crashStateDoc[PSTR("rtcp")] = crashState.rtcp;
    crashStateDoc[PSTR("crashCnt")] = crashState.crashCnt;
    crashStateDoc[PSTR("fSafeMode")] = crashState.fSafeMode;
    
    _crashStateConfig.save(crashStateDoc);
  }
}

#ifdef USE_IOT
void Udawa::_processThingsboardProvisionResponse(const JsonObjectConst &data){
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
            [this](const JsonObjectConst &data) {
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
        for (auto callback : _onThingsboardDisconnectedCallbacks) { 
          callback(); // Call each callback
        }
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
          if(!_iotState.fSharedAttributesSubscribed){
            _iotState.fSharedAttributesSubscribed = _tb.Shared_Attributes_Subscribe(_thingsboardSharedAttributesUpdateCallback);
            if (_iotState.fSharedAttributesSubscribed){
              logger->verbose(PSTR(__func__), PSTR("Thingsboard shared attributes update subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe Thingsboard shared attributes update.\n"));
            }
          }

          if(!_iotState.fRebootRPCSubscribed){
            RPC_Callback rebootCallback("reboot", _thingsboardRPCRebootHandler);
            _iotState.fRebootRPCSubscribed = _tb.RPC_Subscribe(rebootCallback); // Pass the callback directly
            if(_iotState.fRebootRPCSubscribed){
              logger->verbose(PSTR(__func__), PSTR("reboot RPC subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe reboot RPC.\n"));
            }
          }

          if(!_iotState.fConfigSaveRPCSubscribed){
            RPC_Callback configSaveCallback("configSave", _thingsboardRPCConfigSaveHandler);
            _iotState.fConfigSaveRPCSubscribed = _tb.RPC_Subscribe(configSaveCallback); // Pass the callback directly
            if(_iotState.fConfigSaveRPCSubscribed){
              logger->verbose(PSTR(__func__), PSTR("configSave RPC subscribed successfuly.\n"));
            }
            else{
              logger->warn(PSTR(__func__), PSTR("Failed to subscribe configSave RPC.\n"));
            }
          }

          #ifdef USE_IOT_OTA
          _iotState.fIoTCurrentFWSent = _tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION) && _tb.Firmware_Send_State(PSTR("UPDATED"));
          if(_iotState.fIoTCurrentFWSent){
          //if(true){
            _tb.Shared_Attributes_Request(_iotUpdaterFirmwareCheckCallback);
          }
          #endif

          for (auto callback : _onThingsboardConnectedCallbacks) { 
            callback(); // Call each callback
          }
          logger->info(PSTR(__func__),PSTR("IoT Connected!\n"));
        }
      }
      else{
        if(_iotState.fIoTUpdateStarted){
          _tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION) && _tb.Firmware_Send_State(PSTR("UPDATED"));
          if (_tb.Subscribe_Firmware_Update(_iotUpdaterOTACallback) && _tb.Start_Firmware_Update(_iotUpdaterOTACallback)) {
              logger->debug(PSTR(__func__), PSTR("Firmware update started.\n"));
              // Firmware update started successfully
              // Continue with the update process
          } else {
              logger->error(PSTR(__func__), PSTR("Firmware update failed to start.\n"));
              // Handle the update failure
          }
          _iotState.fIoTUpdateStarted = false;
        }
      }
    }

    _tb.loop();
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
#endif

void Udawa::reboot(int countDown = 0){
  crashState.plannedRebootCountDown = countDown;
  crashState.fPlannedReboot = true;
}

void Udawa::_processIoTUpdaterFirmwareCheckAttributesRequest(const JsonObjectConst &data){
  if( _iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && _tb.connected()){
    if( xSemaphoreTake( _iotState.xSemaphoreThingsboard, ( TickType_t ) 5000 ) == pdTRUE )
    {
      if(data["fw_version"] != nullptr){
        logger->info(PSTR(__func__), PSTR("Firmware check local: %s vs cloud: %s\n"), CURRENT_FIRMWARE_VERSION, data["fw_version"].as<const char*>());
        if(strcmp(data["fw_version"].as<const char*>(), CURRENT_FIRMWARE_VERSION)){
          logger->debug(PSTR(__func__), PSTR("Updating firmware...\n"));
          _iotState.fIoTUpdateStarted = true;
        }else{
          logger->debug(PSTR(__func__), PSTR("No need to update firmware.\n"));
          _iotState.fIoTUpdateStarted = false;
        }
      }
      xSemaphoreGive( _iotState.xSemaphoreThingsboard );
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
  if( xSemaphoreTake( _iotState.xSemaphoreThingsboard, ( TickType_t ) 5000 ) == pdTRUE ) {
    logger->debug(PSTR(__func__), PSTR("IoT OTA Progress: %.2f%%\n"),  static_cast<float>(currentChunk * 100U) / totalChuncks);
    xSemaphoreGive( _iotState.xSemaphoreThingsboard );   
  }
}

bool Udawa::iotSendAttributes(const char *buffer){
  bool res = false;
  int length = strlen(buffer);
  if (buffer[length - 1] != '}') {
      logger->verbose(PSTR(__func__),PSTR("The buffer is not JSON formatted!\n"));
      return false;
  }
  if( _iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && _tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( _iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending attribute to broker: %s\n"), buffer);
      res = _tb.sendAttributeJson(buffer);
      xSemaphoreGive( _iotState.xSemaphoreThingsboard );
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
  if( _iotState.xSemaphoreThingsboard != NULL && WiFi.isConnected() && config.state.provSent && _tb.connected() && config.state.accTkn != NULL){
    if( xSemaphoreTake( _iotState.xSemaphoreThingsboard, ( TickType_t ) 10000 ) == pdTRUE )
    {
      logger->verbose(PSTR(__func__), PSTR("Sending telemetry to broker: %s\n"), buffer);
      res = _tb.sendTelemetryJson(buffer); 
      xSemaphoreGive( _iotState.xSemaphoreThingsboard );
    }
    else
    {
      logger->verbose(PSTR(__func__), PSTR("No semaphore available.\n"));
    }   
  }
  return res;
}