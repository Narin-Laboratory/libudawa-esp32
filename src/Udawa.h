#ifndef UDAWA_H
#define UDAWA_H
#include "UdawaLogger.h"
#include "UdawaWiFiHelper.h"
#include <functional> 
#include <vector>
#include "UdawaConfig.h"
#include "secret.h"
#include "params.h"
#include <ESPmDNS.h>
#ifdef USE_WIFI_OTA
#include <ArduinoOTA.h>
#endif
#ifdef USE_LOCAL_WEB_INTERFACE
#include <Crypto.h>
#include <SHA256.h>
#include <mbedtls/sha256.h>
#include <base64.h>
#include <map>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif
#ifdef USE_IOT
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#endif
#ifdef USE_IOT_SECURE
#include <WiFiClientSecure.h>
#else
#include <WiFiClient.h>
#endif

struct CrashState{
    unsigned long rtcp = 0;
    int crashCnt = 0;
    bool fSafeMode = false;
    unsigned long crashStateCheckTimer = millis();
    bool crashStateCheckedFlag = false;
};

#ifdef USE_IOT
struct IoTState{
    TaskHandle_t xHandleIoT;
    BaseType_t xReturnedIoT;
    SemaphoreHandle_t xSemaphoreThingsboard = NULL;
};
class UdawaThingsboardLogger{
    public:
        static void log(const char *error){
            UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
            _logger->debug(PSTR(__func__), PSTR("%s\n"), error);
        }
        
};
#endif

class Udawa {
    public:
        Udawa();
        void run();
        void begin();
        typedef std::function<void(AsyncWebSocket * server, AsyncWebSocketClient * client, 
                          AwsEventType type, void * arg, uint8_t *data, size_t len)> 
                          WsOnEventCallback;
        UdawaLogger *logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        UdawaSerialLogger *serialLogger = UdawaSerialLogger::getInstance(SERIAL_BAUD_RATE);
        UdawaWiFiHelper wiFiHelper;
        UdawaConfig config;
        CrashState crashState;
        String hmacSha256(const String& message, const String& salt);
        #ifdef USE_LOCAL_WEB_INTERFACE
            AsyncWebServer http;
            AsyncWebSocket ws;
            void addOnWsEvent(WsOnEventCallback callback);
        #endif
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
        #ifdef USE_LOCAL_WEB_INTERFACE
            void _onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
            std::vector<WsOnEventCallback> _onWSEventCallbacks;
            std::map<uint32_t, bool> _clientAuthenticationStatus;
            std::map<IPAddress, unsigned long> _clientAuthAttemptTimestamps; 
        #endif
        void _crashStateTruthKeeper(uint8_t direction);
        GenericConfig _crashStateConfig;
        #ifdef USE_IOT
            #ifdef USE_IOT_SECURE
                WiFiClientSecure _tcpClient;
                Arduino_MQTT_Client _mqttClient;
                ThingsBoardSized<UdawaThingsboardLogger> _tb;
            #else
                WiFiClient _tcpClient;
                Arduino_MQTT_Client _mqttClient;
                ThingsBoard _tb;
            #endif
            IoTState _iotState;
            static void _pvTaskCodeThingsboardTaskWrapper(void* pvParameters);
            void _pvTaskCodeThingsboard(void *pvParameters);
            void _processThingsboardProvisionResponse(const Provision_Data &data);
        #endif
};

#endif