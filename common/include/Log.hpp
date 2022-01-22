#pragma once
#include <type_traits>
#include <array>
#include <ostream>
#include <memory>

namespace vwa
{
    class Logger
    {
        // Direct mapping of LogLevel to the corresponding stream. Nullptr if disabled.
        std::array<std::unique_ptr<std::ostream>, 4> streams;

        template <typename T, typename... Args>
        void print(std::ostream &stream, const T &value, const Args &...args)
        {
            stream << value;
            print(stream, args...);
        }

        template <typename T>
        void print(std::ostream &stream, const T &value)
        {
            stream << value;
        }

    public:
        enum LogLevel
        {
            Debug,
            Info,
            Warning,
            Error,
        };

        template <typename... Args>
        void operator()(LogLevel lvl, const Args &...args)
        {
            std::ostream *stream = streams[static_cast<int>(lvl)].get();
            if (stream)
                print(*stream, args...);
        }

        template <LogLevel lvl>
        void setStream(std::unique_ptr<std::ostream> &&stream)
        {
            streams[static_cast<int>(lvl)] = std::move(stream);
        }
    };
}