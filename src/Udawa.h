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
#ifdef USE_WIFI_LOGGER
#include <UdawaWiFiLogger.h>
#endif

struct CrashState{
    unsigned long rtcp = 0;
    int crashCnt = 0;
    bool fSafeMode = false;
    unsigned long crashStateCheckTimer = millis();
    bool crashStateCheckedFlag = false;
    unsigned long plannedRebootTimer = millis();
    unsigned int plannedRebootCountDown = 0;
    bool fPlannedReboot = false;
};

#ifdef USE_IOT
struct IoTState{
    TaskHandle_t xHandleIoT;
    BaseType_t xReturnedIoT;
    SemaphoreHandle_t xSemaphoreThingsboard = NULL;
    bool fSharedAttributesSubscribed = false;
    bool fRPCSubscribed = false;
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
        #ifdef USE_LOCAL_WEB_INTERFACE
        typedef std::function<void(AsyncWebSocket * server, AsyncWebSocketClient * client, 
                          AwsEventType type, void * arg, uint8_t *data, size_t len)> 
                          WsOnEventCallback;
        #endif
        #ifdef USE_IOT
        typedef std::function<void()> ThingsboardOnConnectedCallback;
        typedef std::function<void()> ThingsboardOnDisconnectedCallback;
        typedef std::function<void(const Shared_Attribute_Data &data)> ThingsboardOnSharedAttributesReceivedCallback;
        #endif
        UdawaLogger *logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        UdawaSerialLogger *serialLogger = UdawaSerialLogger::getInstance(SERIAL_BAUD_RATE);
        #ifdef USE_WIFI_LOGGER
        UdawaWiFiLogger *wiFiLogger = UdawaWiFiLogger::getInstance("255.255.255.255", 29514, 256);
        #endif
        UdawaWiFiHelper wiFiHelper;
        UdawaConfig config;
        CrashState crashState;
        #ifdef USE_LOCAL_WEB_INTERFACE
            String hmacSha256(String htP, String salt);
            AsyncWebServer http;
            AsyncWebSocket ws;
            void addOnWsEvent(WsOnEventCallback callback);
        #endif
        #ifdef USE_IOT
            void addOnThingsboardConnected(ThingsboardOnConnectedCallback callback);
            void addOnThingsboardDisconnected(ThingsboardOnDisconnectedCallback callback);
            void addOnThingsboardSharedAttributesReceived(ThingsboardOnSharedAttributesReceivedCallback callback);
        #endif
        void reboot(int countDown);




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
            std::map<uint32_t, bool> _wsClientAuthenticationStatus;
            std::map<IPAddress, unsigned long> _wsClientAuthAttemptTimestamps; 
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
            std::vector<ThingsboardOnConnectedCallback> _onThingsboardConnectedCallbacks;
            std::vector<ThingsboardOnDisconnectedCallback> _onThingsboardDisconnectedCallbacks;
            std::vector<ThingsboardOnSharedAttributesReceivedCallback> _onThingsboardSharedAttributesReceivedCallbacks;
            static void _processThingsboardSharedAttributesUpdateWrapper(void* context, const Shared_Attribute_Data &data) {
                // Retrieve the Udawa instance
                Udawa *instance = static_cast<Udawa*>(context);
                // Call the non-static method using the lambda
                instance->_processThingsboardSharedAttributesUpdate(data); 
            }
            // Declaration of the callback object (within the class)
            Shared_Attribute_Callback _thingsboardSharedAttributesUpdateCallback;
            void _processThingsboardSharedAttributesUpdate(const Shared_Attribute_Data &data);
            
            RPC_Response _processThingsboardRPCReboot(const RPC_Data &data);
            std::function<RPC_Response(const RPC_Data&)> _thingsboardRPCRebootHandler;

            RPC_Response _processThingsboardRPCConfigSave(const RPC_Data &data);
            std::function<RPC_Response(const RPC_Data&)> _thingsboardRPCConfigSaveHandler;
        #endif
};

#endif