#pragma once

#ifdef MERIDIAN_FRAME_TRANSPORT_NO_PCH
    #include <cstdio>
namespace Meridian::Render::Detail
{
    template <class... Args>
    inline void FrameTransportTestLog(std::FILE* a_stream, const char* a_message, Args&&...)
    {
        std::fprintf(a_stream, "%s\n", a_message);
    }
}
    // The NO_PCH unit-test build only needs a visible diagnostic. Keep the
    // production fmt-style call shape without passing its arguments to a
    // printf-style formatter (which produced C4474 warnings).
    #define MERIDIAN_FT_LOG_ERROR(msg, ...) Meridian::Render::Detail::FrameTransportTestLog(stderr, msg __VA_OPT__(, ) __VA_ARGS__)
    #define MERIDIAN_FT_LOG_WARN(msg, ...) Meridian::Render::Detail::FrameTransportTestLog(stderr, msg __VA_OPT__(, ) __VA_ARGS__)
    #define MERIDIAN_FT_LOG_INFO(msg, ...) Meridian::Render::Detail::FrameTransportTestLog(stdout, msg __VA_OPT__(, ) __VA_ARGS__)
#else
    #include <spdlog/spdlog.h>
    #define MERIDIAN_FT_LOG_ERROR(msg, ...) spdlog::error(msg __VA_OPT__(, ) __VA_ARGS__)
    #define MERIDIAN_FT_LOG_WARN(msg, ...) spdlog::warn(msg __VA_OPT__(, ) __VA_ARGS__)
    #define MERIDIAN_FT_LOG_INFO(msg, ...) spdlog::info(msg __VA_OPT__(, ) __VA_ARGS__)
#endif
