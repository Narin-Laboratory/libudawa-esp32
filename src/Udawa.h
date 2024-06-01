#ifndef UDAWA_H
#define UDAWA_H
#include "UdawaLogger.h"
#include "UdawaWiFiHelper.h"
#include <functional> 
#include "UdawaConfig.h"
#include "../../../../../include/secret.h"
#include "../../../../../include/params.h"
#include <ESPmDNS.h>
#ifdef USE_WIFI_OTA
#include <ArduinoOTA.h>
#endif

struct AppState{
    unsigned long rtcp = 0;
    int crashCnt = 0;
    bool fSafeMode = false;
};

class Udawa {
    public:
        Udawa();
        void run();
        void begin();
        UdawaLogger *logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        UdawaSerialLogger *serialLogger = UdawaSerialLogger::getInstance(SERIAL_BAUD_RATE);
        UdawaWiFiHelper wiFiHelper;
        UdawaConfig config;
        AppState state;
    private:
        void _onWiFiConnected();
        void _onWiFiDisconnected();
        void _onWiFiGotIP();
        #ifdef USE_WIFI_OTA
        void _onWiFiOTAStart();
        void _onWiFiOTAEnd();
        void _onWiFiOTAProgress(unsigned int progress, unsigned int total);
        void _onWiFiOTAError(ota_error_t error);
        #endif
};

#endif