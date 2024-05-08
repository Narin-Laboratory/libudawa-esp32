#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "UdawaSerialLogger.h"
#include <Arduino.h>

UdawaSerialLogger *UdawaSerialLogger::_serialLogger = nullptr;

UdawaSerialLogger::UdawaSerialLogger(u_int32_t baudRate) : _baudRate(baudRate) {
    Serial.begin(_baudRate);
}

UdawaSerialLogger *UdawaSerialLogger::getInstance(const uint32_t baudRate)
{
    if(_serialLogger == nullptr)
    {
        _serialLogger = new UdawaSerialLogger(baudRate);
    }
    return _serialLogger;
}

char UdawaSerialLogger::_getErrorChar(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::ERROR:
            return 'E';
        case LogLevel::WARN:
            return 'W';
        case LogLevel::INFO:
            return 'I';
        case LogLevel::DEBUG:
            return 'D';
        case LogLevel::VERBOSE:
            return 'V';
        default:
            return 'X';
    }
}

int UdawaSerialLogger::_getConsoleColorCode(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::ERROR:
            return RED_COLOR_CODE;
        case LogLevel::WARN:
            return YELLOW_COLOR_CODE;
        case LogLevel::INFO:
            return GREEN_COLOR_CODE;
        case LogLevel::DEBUG:
            return CYAN_COLOR_CODE;
        case LogLevel::VERBOSE:
            return MAGENTA_COLOR_CODE;
        default:
            return GREEN_COLOR_CODE;
    }
}

int UdawaSerialLogger::_mapLogLevel(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::NONE:
            return ESP_LOG_NONE;
        case LogLevel::ERROR:
            return ESP_LOG_ERROR;
        case LogLevel::WARN:
            return ESP_LOG_WARN;
        case LogLevel::INFO:
            return ESP_LOG_INFO;
        case LogLevel::DEBUG:
            return ESP_LOG_DEBUG;
        case LogLevel::VERBOSE:
            return ESP_LOG_VERBOSE;
        default:
            return ESP_LOG_VERBOSE;
    }
}

void UdawaSerialLogger::write(const char *tag, LogLevel level, const char *fmt, va_list args)
{
    if(xSemaphoreUdawaSerialLogger == NULL)
    {
        xSemaphoreUdawaSerialLogger = xSemaphoreCreateMutex();
    }

    if(xSemaphoreUdawaSerialLogger != NULL && xSemaphoreTake(xSemaphoreUdawaSerialLogger, (TickType_t) 20))
    {
        //esp_log_level_t esp_log_level = (esp_log_level_t)_mapLogLevel(level);
        esp_log_level_t esp_log_level = ESP_LOG_NONE;
        esp_log_write(esp_log_level, tag, "\033[0;%dm%c (%d) %s: ", _getConsoleColorCode(level), _getErrorChar(level), esp_log_timestamp(), tag);
        esp_log_writev(esp_log_level, tag, fmt, args);
        esp_log_write(esp_log_level, tag, "\033[0m");
        xSemaphoreGive(xSemaphoreUdawaSerialLogger);
    }
    else{
        printf("Could not get semaphore\n");
    }
}