#pragma once

// Common logging facade -- replaces direct spdlog::trace/debug/info/warn/error/critical
// call sites with LOG_TRACE/LOG_DEBUG/LOG_INFO/LOG_WARN/LOG_ERROR/LOG_CRITICAL(...).

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

#ifndef MERIDIAN_LOG_NO_SKSE
    #include <SKSE/SKSE.h>
#endif

namespace Meridian::Log
{
    enum class LogLevel
    {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Err = 4,
        Critical = 5
    };

    [[nodiscard]] inline spdlog::level::level_enum ToSpdlogLevel(LogLevel a_level) noexcept
    {
        return static_cast<spdlog::level::level_enum>(a_level);
    }

    [[nodiscard]] inline bool ShouldDispatch(LogLevel a_level) noexcept
    {
        const auto logger = spdlog::default_logger_raw();
        return logger != nullptr && logger->should_log(ToSpdlogLevel(a_level));
    }

    inline void LogDispatch(LogLevel a_level, const char* a_file, int a_line, const char* a_function, std::string a_message)
    {
        const auto logger = spdlog::default_logger_raw();
        if (logger == nullptr)
        {
            return;
        }

        logger->log(spdlog::source_loc{a_file, a_line, a_function}, ToSpdlogLevel(a_level), a_message);
    }

    // Applies a raw spdlog::level::level_enum value (e.g. from an ini override)
    // to the current default logger.
    inline void SetLevel(int a_spdlogLevelValue)
    {
        if (auto log = spdlog::default_logger(); log != nullptr)
        {
            const auto level = static_cast<spdlog::level::level_enum>(a_spdlogLevelValue);
            log->set_level(level);
            log->flush_on(level);
        }
    }

    namespace Detail
    {
#ifndef MERIDIAN_LOG_NO_SKSE
        // Only available to SKSE-linked targets.
        // For exampleMeridianCEFSubprocess is a standalone executable with no CommonLibSSE/SKSE,
        // so it defines MERIDIAN_LOG_NO_SKSE.
        inline void AddFileSink(std::vector<spdlog::sink_ptr>& a_sinks, const std::string& a_fileStem)
        {
            auto path = SKSE::log::log_directory();
            if (!path)
            {
                SKSE::stl::report_and_fail(std::string_view{"Failed to find standard logging directory"});
            }

            *path /= a_fileStem;
            a_sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }
#endif
    }

    // Options for building and installing one spdlog logger.
    struct InitOptions
    {
        std::string name;                         // spdlog registry name
        std::optional<std::string> logFileStem;   // release builds (or alwaysAddFileSink): log_directory()/<stem>
        bool useDebugMsvcSink = true;             // _DEBUG builds: add an msvc_sink_mt
        bool alwaysAddFileSink = false;           // add the file sink even in _DEBUG builds (in addition to the msvc sink)
        std::vector<spdlog::sink_ptr> extraSinks; // always added first (e.g. CEFSubprocess's IPC-relay sink)
        bool makeDefault = true;                  // true: spdlog::set_default_logger; false: spdlog::register_logger
        std::string pattern = "[%Y-%m-%d %T.%e] [%t] [%l] [%s:%#] %v";
        LogLevel debugLevel = LogLevel::Trace;
        LogLevel releaseLevel = LogLevel::Info;
    };

    // Builds a logger from a_options and installs it (as the default logger, or
    // registered under its own name), returning it so callers may retain a
    // handle (e.g. CEFSubprocess re-points its IPC sink's browser later).
    // Returns nullptr (never throws) if sink construction fails, e.g. the log
    // file couldn't be created -- callers that need init to be a hard
    // precondition can check for that.
    [[nodiscard]] inline std::shared_ptr<spdlog::logger> Init(const InitOptions& a_options) noexcept
    try
    {
        std::vector<spdlog::sink_ptr> sinks = a_options.extraSinks;
        LogLevel level;

#ifdef _DEBUG
        level = a_options.debugLevel;
        if (a_options.useDebugMsvcSink)
        {
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
        }
    #ifndef MERIDIAN_LOG_NO_SKSE
        if (a_options.alwaysAddFileSink && a_options.logFileStem)
        {
            Detail::AddFileSink(sinks, *a_options.logFileStem);
        }
    #endif
#else
        level = a_options.releaseLevel;
    #ifndef MERIDIAN_LOG_NO_SKSE
        if (a_options.logFileStem)
        {
            Detail::AddFileSink(sinks, *a_options.logFileStem);
        }
    #endif
#endif

        auto log = std::make_shared<spdlog::logger>(a_options.name, sinks.begin(), sinks.end());
        log->set_level(ToSpdlogLevel(level));
        log->flush_on(ToSpdlogLevel(level));
        log->set_pattern(a_options.pattern);

        if (a_options.makeDefault)
        {
            spdlog::set_default_logger(log);
        }
        else
        {
            spdlog::register_logger(log);
        }

        return log;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

#define LOG_AT(level_enum, ...)                                                                                       \
    do                                                                                                                \
    {                                                                                                                 \
        if (::Meridian::Log::ShouldDispatch(level_enum))                                                              \
        {                                                                                                             \
            ::Meridian::Log::LogDispatch((level_enum), __FILE__, __LINE__, __FUNCTION__, ::fmt::format(__VA_ARGS__)); \
        }                                                                                                             \
    } while (0)

#define LOG_TRACE(...) LOG_AT(::Meridian::Log::LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_AT(::Meridian::Log::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) LOG_AT(::Meridian::Log::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) LOG_AT(::Meridian::Log::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) LOG_AT(::Meridian::Log::LogLevel::Err, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_AT(::Meridian::Log::LogLevel::Critical, __VA_ARGS__)
