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