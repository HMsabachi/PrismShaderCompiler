#pragma once

#include <string>
#include <format>
#include <functional>
#include <cstdint>

namespace PrismShaderCompiler
{

enum class LogLevel : uint8_t
{
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
};

using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

namespace Callbacks
{
    inline void NullLog(LogLevel, const std::string&) {}
}

    class Log
    {
    public:
        static Log& Instance()
        {
            static Log s_Instance;
            return s_Instance;
        }

        void SetCallback(LogCallback cb) { m_Callback = std::move(cb); }

        template<typename... Args>
        void Debug(std::format_string<Args...> fmt, Args&&... args)
        {
            if (m_Callback)
                m_Callback(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void Info(std::format_string<Args...> fmt, Args&&... args)
        {
            if (m_Callback)
                m_Callback(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void Warn(std::format_string<Args...> fmt, Args&&... args)
        {
            if (m_Callback)
                m_Callback(LogLevel::Warning, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void Error(std::format_string<Args...> fmt, Args&&... args)
        {
            if (m_Callback)
                m_Callback(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void Fatal(std::format_string<Args...> fmt, Args&&... args)
        {
            if (m_Callback)
                m_Callback(LogLevel::Fatal, std::format(fmt, std::forward<Args>(args)...));
        }

    private:
        Log() = default;
        LogCallback m_Callback = Callbacks::NullLog;
    };

} // namespace PrismShaderCompiler
