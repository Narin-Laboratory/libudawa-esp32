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
    logger->debug(PSTR(__func__), PSTR("Initializing LittleFS: %d\n"), config.begin());
    config.load();
    
    logger->setLogLevel((LogLevel)config.state.logLev);
    #ifdef USE_WIFI_LOGGER
    wiFiLogger->setConfig(config.state.logIP, config.state.logPort, WIFI_LOGGER_BUFFER_SIZE);
    #endif

    #ifdef USE_I2C
    Wire.begin();
    Wire.setClock(400000);
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

void Udawa::reboot(int countDown = 0){
  crashState.plannedRebootCountDown = countDown;
  crashState.fPlannedReboot = true;
}

void Udawa::_onWiFiConnected(){
    
}

void Udawa::_onWiFiDisconnected(){

}

void Udawa::_onWiFiGotIP(){
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
        /*if(err != DeserializationError::Ok){
          logger->error(PSTR(__func__), PSTR("Failed to parse JSON.\n"));
          return;
        }*/
        
        // If client is not authenticated, check credentials
        if(!_wsClientAuthenticationStatus[client->id()]) {
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
            //logger->debug(PSTR(__func__), PSTR("\n\tserver: %s\n\tclient: %s\n\tkey: %s\n\tsalt: %s\n"), _auth.c_str(), auth.c_str(), config.state.htP, salt.c_str());
            if (_auth == auth) {
              // Client authenticated successfully, update the status in the map
              _wsClientAuthenticationStatus[client->id()] = true;
              client->printf(PSTR("{\"status\": {\"code\": 200, \"msg\": \"Authorized.\", \"model\": \"%s\"}}"), config.state.model);
              logger->debug(PSTR(__func__), PSTR("ws [%u] authenticated.\n"), client->id());
              syncClientAttr(2);
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
#endif

void Udawa::rtcUpdate(long ts){
  #ifdef USE_HW_RTC
  crashState.fRTCHwDetected = false;
  if(!_hwRTC.begin()){
    logger->error(PSTR(__func__), PSTR("RTC module not found; please update the device time manually. Any function that requires precise timing will malfunction! \n"));
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
    JsonObject cfg = doc["cfg"].to<JsonObject>(); 
    cfg[PSTR("name")] = config.state.name;
    cfg[PSTR("model")] = config.state.model;
    cfg[PSTR("group")] = config.state.group;
    cfg[PSTR("ap")] = WiFi.SSID();
    cfg[PSTR("gmtOff")] = config.state.gmtOff;
    cfg[PSTR("hname")] = config.state.hname;
    serializeJson(doc, buffer);
    wsBroadcast(buffer);
  }
  #endif

  for (auto callback : _onSyncClientAttributesCallback) { 
    callback(direction); // Call each callback
  }
}