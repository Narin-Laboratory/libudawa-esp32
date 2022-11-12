#include "logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

LogManager *LogManager::_log_manager = nullptr;

LogManager *LogManager::GetInstance(const LogLevel log_level)
{
    if(_log_manager == nullptr)
    {
        _log_manager = new LogManager(log_level);
    }
    return _log_manager;
}

void LogManager::dispatch_message(const char *tag, const LogLevel level, const char *fmt, va_list args)
{
    if(_log_level >= level)
    {
        for(auto& log_handler: _log_handlers)
        {
            log_handler->log_message(tag, level, fmt, args);
        }
    }
}

void LogManager::add_logger(ILogHandler *log_handler)
{
    _log_handlers.push_back(log_handler);
}

void LogManager::remove_logger(ILogHandler *log_handler)
{
    _log_handlers.remove(log_handler);
}

void LogManager::verbose(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dispatch_message(tag, LogLevel::VERBOSE, fmt, args);
    va_end(args);
}

void LogManager::debug(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dispatch_message(tag, LogLevel::DEBUG, fmt, args);
    va_end(args);
}

void LogManager::info(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dispatch_message(tag, LogLevel::INFO, fmt, args);
    va_end(args);
}

void LogManager::warn(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dispatch_message(tag, LogLevel::WARN, fmt, args);
    va_end(args);
}

void LogManager::error(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    dispatch_message(tag, LogLevel::ERROR, fmt, args);
    va_end(args);
}

void LogManager::set_log_level(const char *tag, LogLevel level)
{
    _log_level = level;
}