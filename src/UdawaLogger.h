#ifndef UDAWALOGGER_H
#define UDAWALOGGER_H

#include <Arduino.h>
#include <stdarg.h>
#include <list>

enum class LogLevel
{
    NONE,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    VERBOSE,
};

class ILogHandler
{
    public:
        ILogHandler();
        ILogHandler(uint32_t baudRate);
        virtual void write(const char *tag, const LogLevel level, const char *fmt, va_list args) = 0;
};

class UdawaLogger
{
    public:
        void operator = (const UdawaLogger &) = delete;
        static UdawaLogger *getInstance(const LogLevel logLevel = LogLevel::VERBOSE);

        void addLogger(ILogHandler *logHandler);
        void removeLogger(ILogHandler *logHandler);
        void verbose(const char *tag, const char *fmt, ...);
        void debug(const char *tag, const char *fmt, ...);
        void info(const char *tag, const char *fmt, ...);
        void warn(const char *tag, const char *fmt, ...);
        void error(const char *tag, const char *fmt, ...);
        void setLogLevel(LogLevel level);
        
    private:
        UdawaLogger(const LogLevel logLevel) : _logLevel(logLevel){}
        void dispatchMessage(const char *tag, const LogLevel level, const char *fmt, va_list args);
        std::list<ILogHandler*> _logHandlers;
        static UdawaLogger *_logManager;
        LogLevel _logLevel;
};

#endif