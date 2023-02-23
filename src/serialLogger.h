/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Function helper library for ESP32 based UDAWA multi-device firmware development
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/

#ifndef SERIALLOGGER_H
#define SERIALLOGGER_H

#include <stdarg.h>
#include <logging.h>

class ESP32SerialLogger : public ILogHandler
{
    public:
        void log_message(const char *tag, const LogLevel level, const char *fmt, va_list args) override;
};

#endif