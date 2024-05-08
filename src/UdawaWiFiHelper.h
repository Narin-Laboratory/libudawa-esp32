#ifndef UDAWAWIFIHELPER_H
#define UDAWAWIFIHELPER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "UdawaLogger.h"
#include "UdawaSerialLogger.h"
#include <vector>

class UdawaWiFiHelper{
    public:
        UdawaWiFiHelper();
        typedef std::function<void()> WiFiConnectedCallback;
        typedef std::function<void()> WiFiDisconnectedCallback;
        typedef std::function<void()> WiFiGotIPCallback;
        void addOnConnectedCallback(WiFiConnectedCallback callback);
        void addOnDisconnectedCallback(WiFiDisconnectedCallback callback);
        void addOnGotIPCallback(WiFiGotIPCallback callback);
        void begin(const char* wssid, const char* wpass,
            const char* dssid, const char* dpass, const char* hname);
        void run();
    private:
        WiFiMulti _wiFi;
        UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        const char* _wssid;
        const char* _wpass;
        const char* _dssid;
        const char* _dpass;
        const char* _hname;
        void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
        std::vector<WiFiConnectedCallback> _onConnectedCallbacks;
        std::vector<WiFiDisconnectedCallback> _onDisconnectedCallbacks;
        std::vector<WiFiGotIPCallback> _onGotIPCallbacks;
};

#endif