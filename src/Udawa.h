#ifndef UDAWA_H
#define UDAWA_H
#include "params.h"
#include "UdawaLogger.h"
#include "UdawaWiFiHelper.h"
#include <functional> 
#include <vector>
#include "UdawaConfig.h"
#include "secret.h"
#include <ESP32Time.h>
#ifdef THINGSBOARD_ENABLE_STREAM_UTILS
#include <StreamUtils.h>
#endif
#include <ESPmDNS.h>
#ifdef USE_WIFI_OTA
#include <ArduinoOTA.h>
#endif
#ifdef USE_IOT
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#ifdef USE_IOT_OTA
#include <Espressif_Updater.h>
#endif
#endif
#ifdef USE_IOT_SECURE
#include <WiFiClientSecure.h>
#else
#include <WiFiClient.h>
#endif
#ifdef USE_WIFI_LOGGER
#include <UdawaWiFiLogger.h>
#endif
#include <NTPClient.h>
#include <WiFiUdp.h>
#ifdef USE_I2C
#include <Wire.h>
#endif
#ifdef USE_HW_RTC
#include <ErriezDS3231.h>
#endif
#ifdef USE_LOCAL_WEB_INTERFACE
#include <Crypto.h>
#include <SHA256.h>
#include <mbedtls/sha256.h>
#include <base64.h>
#include <map>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
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
    bool fRTCHwDetected = false;
};

#ifdef USE_IOT
struct IoTState{
    TaskHandle_t xHandleIoT;
    BaseType_t xReturnedIoT;
    SemaphoreHandle_t xSemaphoreThingsboard = NULL;
    bool fSharedAttributesSubscribed = false;
    bool fRebootRPCSubscribed = false;
    bool fConfigSaveRPCSubscribed = false;
    bool fIoTCurrentFWSent = false;
    bool fIoTUpdateRequestSent = false;
    bool fIoTUpdateStarted = false;
};
class UdawaThingsboardLogger{
    public:
        static void log(const char *error){
            UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
            _logger->debug(PSTR(__func__), PSTR("%s\n"), error);
        }
        template<typename ...Args>
        static int printfln(char const * const format, Args const &... args){
            UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
            size_t len = strlen(format);
            char newFormat[len + 2];  // +2 for '\n' and null terminator
            strcpy(newFormat, format);
            strcat(newFormat, "\n");

            _logger->debug(PSTR(__func__), newFormat, args...);
            return 1U;
        }
        static int println(char const * const message){
            UdawaLogger *_logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
            _logger->debug(PSTR(__func__), PSTR("%s\n"), message);
            return 1U;
        }
};
#ifdef USE_IOT_OTA
constexpr std::array<const char*, 1U> REQUESTED_FW_CHECK_SHARED_ATTRIBUTES = {
    FW_VER_KEY
};

template <size_t N>
Attribute_Request_Callback createFirmwareCheckCallback(
    std::function<void(const JsonObjectConst&)> callback,
    const std::array<const char*, N>& attributes) {
    return Attribute_Request_Callback(callback, attributes.begin(), attributes.end());
}
#endif
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
        typedef std::function<void(const JsonObjectConst &data)> ThingsboardOnSharedAttributesReceivedCallback;
        bool iotSendAttributes(const char *buffer);
        bool iotSendTelemetry(const char *buffer);
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
            void wsBroadcast(const char *buffer);
            SemaphoreHandle_t xSemaphoreWSBroadcast;
        #endif
        #ifdef USE_IOT
            void addOnThingsboardConnected(ThingsboardOnConnectedCallback callback);
            void addOnThingsboardDisconnected(ThingsboardOnDisconnectedCallback callback);
            void addOnThingsboardSharedAttributesReceived(ThingsboardOnSharedAttributesReceivedCallback callback);
            #ifdef USE_IOT_SECURE
                WiFiClientSecure _tcpClient;
                Arduino_MQTT_Client _mqttClient;
                ThingsBoardSized<UdawaThingsboardLogger> tb;
            #else
                WiFiClient _tcpClient;
                Arduino_MQTT_Client _mqttClient;
                ThingsBoardSized<UdawaThingsboardLogger> tb;
            #endif
            IoTState iotState;
        #endif
        void reboot(int countDown);
        ESP32Time RTC;
        void rtcUpdate(long ts);
        void syncClientAttr(uint8_t direction);
        typedef std::function<void(uint8_t direction)> SyncClientAttributesCallback;
        void addOnSyncClientAttributesCallback(SyncClientAttributesCallback callback);
        std::vector<SyncClientAttributesCallback> _onSyncClientAttributesCallback;

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
            static void _pvTaskCodeThingsboardTaskWrapper(void* pvParameters);
            void _pvTaskCodeThingsboard(void *pvParameters);
            void _processThingsboardProvisionResponse(const JsonObjectConst &data);
            std::vector<ThingsboardOnConnectedCallback> _onThingsboardConnectedCallbacks;
            std::vector<ThingsboardOnDisconnectedCallback> _onThingsboardDisconnectedCallbacks;
            std::vector<ThingsboardOnSharedAttributesReceivedCallback> _onThingsboardSharedAttributesReceivedCallbacks;
            static void _processThingsboardSharedAttributesUpdateWrapper(void* context, const JsonObjectConst &data) {
                // Retrieve the Udawa instance
                Udawa *instance = static_cast<Udawa*>(context);
                // Call the non-static method using the lambda
                instance->_processThingsboardSharedAttributesUpdate(data);
            }
            // Declaration of the callback object (within the class)
            Shared_Attribute_Callback _thingsboardSharedAttributesUpdateCallback;
            void _processThingsboardSharedAttributesUpdate(const JsonObjectConst &data);
            
            void _processThingsboardRPCReboot(const JsonVariantConst &data, JsonDocument &response);
            std::function<void(const JsonVariantConst &data, JsonDocument &response)> _thingsboardRPCRebootHandler;

            void _processThingsboardRPCConfigSave(const JsonVariantConst &data, JsonDocument &response);
            std::function<void(const JsonVariantConst &data, JsonDocument &response)> _thingsboardRPCConfigSaveHandler;
            #ifdef USE_IOT_OTA
            Espressif_Updater _iotUpdater;
            void _iotUpdaterUpdatedCallback(const bool& success);
            void _iotUpdaterProgressCallback(const size_t& currentChunk, const size_t& totalChuncks);
            void _processIoTUpdaterFirmwareCheckAttributesRequest(const JsonObjectConst &data);
            const Attribute_Request_Callback _iotUpdaterFirmwareCheckCallback; //(&_processIoTUpdaterFirmwareCheckAttributesRequest, "fw_version");
            const OTA_Update_Callback _iotUpdaterOTACallback; //(&_iotUpdaterProgressCallback, &_iotUpdaterUpdatedCallback, CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION, &_iotUpdater, IOT_FIRMWARE_FAILURE_RETRIES, IOT_FIRMWARE_PACKET_SIZE);            
            #endif
        #endif
        #ifdef USE_HW_RTC
        ErriezDS3231 _hwRTC;
        #endif
};

#endif