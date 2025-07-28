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
#include <../lib/Crypto/src/Crypto.h>
#include <../lib/Crypto/src/SHA256.h>
#include <mbedtls/sha256.h>
#include <base64.h>
#include <map>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif
#include <ArduinoHttpClient.h>
#include <Update.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))

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
    unsigned long lastRecordedDatetime = 0;
    unsigned long lastRecordedDatetimeSavedTimer = 0;
    bool fFSDownloading = false;
    bool fStartServices = false;
    bool fStopServices = false;
    bool fDoInit = false;
};

struct AlarmMessage
{
    uint16_t code;
    uint8_t color; 
    int32_t blinkCount; 
    uint16_t blinkDelay;
};

class Udawa {
    public:
        static Udawa* getInstance();
        void run();
        void begin();
        void FSDownloader();
        #ifdef USE_LOCAL_WEB_INTERFACE
        typedef std::function<void(AsyncWebSocket * server, AsyncWebSocketClient * client, 
                          AwsEventType type, void * arg, uint8_t *data, size_t len)> 
                          WsOnEventCallback;
        #endif
        UdawaLogger *logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
        UdawaSerialLogger *serialLogger = UdawaSerialLogger::getInstance(SERIAL_BAUD_RATE);
        #ifdef USE_WIFI_LOGGER
        UdawaWiFiLogger *wiFiLogger = UdawaWiFiLogger::getInstance("255.255.255.255", 29514, 1024);
        #endif
        UdawaWiFiHelper wiFiHelper;
        UdawaConfig config;
        CrashState crashState;
        void setAlarm(uint16_t code, uint8_t color, int32_t blinkCount, uint16_t blinkDelay);

        #ifdef USE_LOCAL_WEB_INTERFACE
            String hmacSha256(String htP, String salt);
            AsyncWebServer http;
            AsyncWebSocket ws;
            void addOnWsEvent(WsOnEventCallback callback);
            void wsBroadcast(const char *buffer);
            void wsBroadcast(DynamicJsonDocument &doc);
            SemaphoreHandle_t xSemaphoreWSBroadcast;
        #endif
        void reboot(int countDown);
        ESP32Time RTC;
        void rtcUpdate(long ts);
        void syncClientAttr(uint8_t direction);
        typedef std::function<void(uint8_t direction)> SyncClientAttributesCallback;
        void addOnSyncClientAttributesCallback(SyncClientAttributesCallback callback);
        std::vector<SyncClientAttributesCallback> _onSyncClientAttributesCallback;

        typedef std::function<void()> FSDownloadedCallback;
        void addOnFSDownloadedCallback(FSDownloadedCallback callback);
        std::vector<FSDownloadedCallback> _onFSDownloadedCallback;

        void I2CScanner(DynamicJsonDocument &doc);
        void I2CScanner();

    private:
        static Udawa* instance;
        Udawa();
        void _onWiFiConnected();
        void _onWiFiDisconnected();
        void _onWiFiGotIP();
        void _onWiFiAPNewClientIP();
        void _onWiFiAPStart();
        void _doInit();
        void _startServices();
        void _stopServices();
        void _setFInit(bool fInit);
        void _setLEDBuzzer(uint8_t color, uint8_t isBlink, int32_t blinkCount, uint16_t blinkDelay);
        static void _alarmTaskRoutine(void *arg);
        TaskHandle_t _xHandleAlarm = NULL;
        BaseType_t _xReturnedAlarm;
        QueueHandle_t _xQueueAlarm;
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
            std::map<uint32_t, String> _wsClientSalts;
        #endif
        void _crashStateTruthKeeper(uint8_t direction);
        GenericConfig _crashStateConfig;
        #ifdef USE_HW_RTC
        ErriezDS3231 _hwRTC;
        #endif
};

#endif