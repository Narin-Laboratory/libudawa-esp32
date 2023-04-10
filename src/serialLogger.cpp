/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Function helper library for ESP32 based UDAWA multi-device firmware development
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "serialLogger.h"

#define RED_COLOR_CODE 31
#define GREEN_COLOR_CODE 32
#define YELLOW_COLOR_CODE 33
#define MAGENTA_COLOR_CODE 35
#define CYAN_COLOR_CODE 34
#define WHITE_COLOR_CODE 37

static SemaphoreHandle_t xSemaphore = NULL;

char get_error_char(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::CODE:
            return 'E';
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

int get_console_color_code(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::CODE:
            return WHITE_COLOR_CODE;
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

int map_log_level(const LogLevel level)
{
    switch(level)
    {
        case LogLevel::NONE:
            return ESP_LOG_NONE;
        case LogLevel::CODE:
            return ESP_LOG_CODE;
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

void ESP32SerialLogger::log_message(const char *tag, LogLevel level, const char *fmt, va_list args)
{
    if(xSemaphore == NULL)
    {
        xSemaphore = xSemaphoreCreateMutex();
    }

    if(xSemaphore != NULL && xSemaphoreTake(xSemaphore, (TickType_t) 20))
    {
        //esp_log_level_t esp_log_level = (esp_log_level_t)map_log_level(level);
        esp_log_level_t esp_log_level = ESP_LOG_ERROR;
        esp_log_write(esp_log_level, tag, "\033[0;%dm%c (%d) %s: ", get_console_color_code(level), get_error_char(level), esp_log_timestamp(), tag);
        esp_log_writev(esp_log_level, tag, fmt, args);
        esp_log_write(esp_log_level, tag, "\033[0m");
        xSemaphoreGive(xSemaphore);
    }
    else{
        printf("Could not get semaphore\n");
    }
}