#include "UdawaWiFiHelper.h"

UdawaWiFiHelper::UdawaWiFiHelper() {

    }

void UdawaWiFiHelper::begin(const char* wssid, const char* wpass,
    const char* dssid, const char* dpass, const char* hname){
    _wssid = wssid; 
    _wpass = wpass; 
    _dssid = dssid;
    _dpass = dpass;
    _hname = hname;
    
    _logger->debug(PSTR(__func__), PSTR("Initializing WiFi network...\n"));
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hname);
    WiFi.setAutoReconnect(true);

    _wiFi.addAP(_wssid, _wpass);
    _wiFi.addAP(_dssid, _dpass);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info){
        this->onWiFiEvent(event, info);
    });
}

void UdawaWiFiHelper::addOnConnectedCallback(WiFiConnectedCallback callback) {

    _onConnectedCallbacks.push_back(callback);
}

void UdawaWiFiHelper::addOnDisconnectedCallback(WiFiDisconnectedCallback callback) {
    _onDisconnectedCallbacks.push_back(callback);
}

void UdawaWiFiHelper::addOnGotIPCallback(WiFiGotIPCallback callback) {
    _onGotIPCallbacks.push_back(callback);
}

void UdawaWiFiHelper::run(){
    _wiFi.run();
}

void UdawaWiFiHelper::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  // Implement your event handling logic here
  // For example, you can print event details
  if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    _logger->debug(PSTR(__func__), PSTR("WiFi network %s disconnected!\n"), info.wifi_sta_connected.ssid);
    for (auto callback : _onDisconnectedCallbacks) { 
        callback(); // Call each callback
    }
  }
  else if(event == ARDUINO_EVENT_WIFI_STA_CONNECTED){
    _logger->debug(PSTR(__func__), PSTR("WiFi network %s connected!\n"), info.wifi_sta_connected.ssid);
    for (auto callback : _onConnectedCallbacks) { 
        callback(); // Call each callback
    }
  }
  else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP){
    _logger->debug(PSTR(__func__), PSTR("WiFi network got IP: %s.\n"), WiFi.localIP().toString().c_str());
    for (auto callback : _onGotIPCallbacks) { 
        callback(); // Call each callback
    }
  }
}