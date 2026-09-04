#pragma once

#include <optional>
#include <string>

#include "MeridianUIAPI/Settings.h"

namespace Meridian::Config
{
    enum class CompositorTiming
    {
        AfterRendererEnd,
        BeforeRendererEnd
    };

    const char* ToString(CompositorTiming a_timing) noexcept;

    struct IniOverrides
    {
        std::optional<Meridian::UI::RendererType> rendererType;
        std::optional<CompositorTiming> compositorTiming;
        std::optional<bool> remoteDebuggingEnabled;
        std::optional<int> remoteDebuggingPort;
        std::optional<bool> nativeMenuLangSwitching;
        std::optional<bool> allowRemoteContent;
        std::optional<int> logLevel;  // spdlog::level::level_enum value
    };

    /// <summary>
    /// Pure parse: reads MeridianUI.ini text from memory into overrides. No
    /// file or logging I/O — safe to unit test in isolation.
    /// </summary>
    IniOverrides ParseIni(const char* a_iniText);

#ifndef MERIDIAN_INICONFIG_NO_RUNTIME
    /// <summary>
    /// Reads Data\SKSE\Plugins\MeridianUI.ini once (relative to the game's
    /// current working directory) and caches the parsed overrides for the
    /// rest of the session. A missing file yields empty overrides silently;
    /// a found file logs one info line with the resolved path.
    /// </summary>
    const IniOverrides& LoadIniOverrides();
#endif

    /// <summary>
    /// Applies present overrides onto a_settings; absent overrides leave the
    /// existing field untouched. RemoteDebuggingEnabled=false forces port 0;
    /// RemoteDebuggingEnabled=true permits a same-file RemoteDebuggingPort
    /// override. A port alone never enables debugging.
    /// </summary>
    void ApplyOverrides(const IniOverrides& a_overrides, Meridian::UI::Settings& a_settings);
}
