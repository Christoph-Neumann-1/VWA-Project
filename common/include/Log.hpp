#pragma once
#include <type_traits>
#include <array>
#include <ostream>
#include <memory>

#define ErrorColor "\033[1;31m"
#define WarningColor "\033[1;33m"
#define DebugColor "\033[1;32m"
#define InfoColor "\033[1;34m"
#define ResetColor "\033[0m"

#define ColorE(str) ErrorColor str ResetColor
#define ColorW(str) WarningColor str ResetColor
#define ColorD(str) DebugColor str ResetColor
#define ColorI(str) InfoColor str ResetColor

namespace vwa
{
    class Logger
    {
        // Direct mapping of LogLevel to the corresponding stream. Nullptr if disabled.
        std::array<std::ostream *, 4> streams;

    public:
        // constexpr static const char *const errorColor = "\033[1;31m";
        // constexpr static const char *const warningColor = "\033[1;33m";
        // constexpr static const char *const infoColor = "\033[1;34m";
        // constexpr static const char *const debugColor = "\033[1;32m";
        // constexpr static const char *const resetColor = "\033[0m";

        enum LogLevel
        {
            Error,
            Warning,
            Info,
            Debug,
        };

        // The following requires clauses are there because c++ doesn't allow explicit specialization here, this bypasses it

        template <typename T>
        requires(!std::is_same_v<T, LogLevel>)
            Logger &
            operator<<(T &&t)
        {
            if (auto stream = streams[static_cast<uint>(logLevel)])
                (*stream) << std::forward<T>(t);
            return *this;
        }

        template <typename T>
        requires(!std::is_same_v<T, LogLevel>)
            Logger &
            operator<<(const T &t)
        {
            if (auto stream = streams[static_cast<uint>(logLevel)])
                (*stream) << t;
            return *this;
        }

        // TODO: check for performance issues and write more efficient implementation
        template <typename T>
        requires std::is_same_v<T, LogLevel>
            Logger &operator<<(T lvl)
        {
            auto stream = streams[static_cast<uint>(lvl)];
            if (stream)
                switch (lvl)
                {
                case LogLevel::Error:
                    (*stream) << ColorE("Error: ");
                    break;
                case LogLevel::Warning:
                    (*stream) << ColorW("Warning: ");
                    break;
                case LogLevel::Debug:
                    (*stream) << ColorD("Debug: ");
                    break;
                case LogLevel::Info:
                    (*stream) << ColorI("Info: ");
                }
            logLevel = lvl;
            return *this;
        }

        void setLogLevel(LogLevel lvl)
        {
            logLevel = lvl;
        }

        std::ostream *getStream(LogLevel lvl)
        {
            return streams[static_cast<int>(lvl)];
        }

        void setStream(LogLevel lvl, std::ostream *stream)
        {
            streams[static_cast<int>(lvl)] = stream;
        }

    private:
        LogLevel logLevel = LogLevel::Info;
    };
}