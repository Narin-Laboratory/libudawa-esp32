#include "Udawa.h"

Udawa::Udawa() : config("/config.json") {
    logger->addLogger(serialLogger);
    logger->setLogLevel(LogLevel::VERBOSE);
}

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