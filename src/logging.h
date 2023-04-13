/**
 * UDAWA - Universal Digital Agriculture Watering Assistant
 * Function helper library for ESP32 based UDAWA multi-device firmware development
 * Licensed under aGPLv3
 * Researched and developed by PRITA Research Group & Narin Laboratory
 * prita.undiknas.ac.id | narin.co.id
**/

#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>
#include <list>

enum class LogLevel
{
    NONE,
    ALARM,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    VERBOSE
};

class ILogHandler
{
    public:
        virtual void log_message(const char *tag, const LogLevel level, const char *fmt, va_list args) = 0;
};

class LogManager
{
    private:
        void dispatch_message(const char *tag, const LogLevel level, const char *fmt, va_list args);
        LogManager(const LogLevel log_level) : _log_level(log_level){}
        std::list<ILogHandler*> _log_handlers;
        static LogManager *_log_manager;
        LogLevel _log_level;

    public:
        LogManager(LogManager &other) = delete;
        void operator = (const LogManager &) = delete;
        static LogManager *GetInstance(const LogLevel log_level = LogLevel::VERBOSE);

        void add_logger(ILogHandler *log_handler);
        void remove_logger(ILogHandler *log_handler);
        void verbose(const char *tag, const char *fmt, ...);
        void debug(const char *tag, const char *fmt, ...);
        void info(const char *tag, const char *fmt, ...);
        void warn(const char *tag, const char *fmt, ...);
        void error(const char *tag, const char *fmt, ...);
        void alarm(const char *tag, const char *fmt, ...);
        void set_log_level(const char *tag, LogLevel level);
};

#endif