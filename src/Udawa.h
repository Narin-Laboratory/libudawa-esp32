#ifndef UDAWA_H
#define UDAWA_H
#include "UdawaLogger.h"
#include "UdawaWiFiHelper.h"
#include <functional> 


class Udawa {
    public:
        Udawa();
        void run();
        void begin();
    private:
        UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        UdawaWiFiHelper _wiFiHelper;
        void _onWiFiConnected();
        void _onWiFiDisconnected();
        void _onWiFiGotIP();
};

#endif