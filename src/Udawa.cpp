#include "Udawa.h"

#ifdef USE_LOCAL_WEB_INTERFACE
Udawa::Udawa() : config(PSTR("/config.json")), http(80), ws(PSTR("/ws")) {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);
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

    wiFiHelper.begin(config.state.wssid, config.state.wpass, config.state.dssid, config.state.dpass, config.state.model);
    wiFiHelper.addOnConnectedCallback(std::bind(&Udawa::_onWiFiConnected, this));
    wiFiHelper.addOnGotIPCallback(std::bind(&Udawa::_onWiFiGotIP, this));
    wiFiHelper.addOnDisconnectedCallback(std::bind(&Udawa::_onWiFiDisconnected, this));

    logger->info(PSTR(__func__), PSTR("Firmware version %s compiled on %s.\n"), CURRENT_FIRMWARE_VERSION, COMPILED);


    state.rtcp = millis();
    if(state.rtcp < 100){
        state.crashCnt++;
        if(state.crashCnt >= 60){
            state.fSafeMode = true;
            logger->warn(PSTR(__func__), PSTR("** SAFEMODE ACTIVATED **\n"));
        }
    }
    logger->debug(PSTR(__func__), PSTR("Runtime Counter: %d, Crash Counter: %d, Safemode Status: %s\n"), state.rtcp, state.crashCnt, state.fSafeMode ? PSTR("ENABLED") : PSTR("DISABLED"));
    
}

void Udawa::run(){
    wiFiHelper.run();

    #ifdef USE_WIFI_OTA
    ArduinoOTA.handle();
    #endif

    #ifdef USE_LOCAL_WEB_INTERFACE
    ws.cleanupClients();
    
    #endif
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
#endif

void Udawa::addOnWsEvent(WsOnEventCallback callback) {
    _onWSEventCallbacks.push_back(callback);
}

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