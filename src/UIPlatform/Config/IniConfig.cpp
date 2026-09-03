#include "IniConfig.h"

#include <SimpleIni.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <string_view>

#ifndef MERIDIAN_INICONFIG_NO_RUNTIME
    #include <filesystem>
    #include <fstream>
    #include <sstream>

    #include <spdlog/spdlog.h>
    #include "Utils/PathUtils.h"
#endif

namespace
{
    bool EqualsCaseInsensitive(std::string_view a_lhs, std::string_view a_rhs)
    {
        if (a_lhs.size() != a_rhs.size())
        {
            return false;
        }
        return std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](unsigned char a_a, unsigned char a_b) {
            return std::tolower(a_a) == std::tolower(a_b);
        });
    }

    std::optional<bool> ParseBool(std::string_view a_value)
    {
        if (a_value == "1" || EqualsCaseInsensitive(a_value, "true") || EqualsCaseInsensitive(a_value, "yes"))
        {
            return true;
        }
        if (a_value == "0" || EqualsCaseInsensitive(a_value, "false") || EqualsCaseInsensitive(a_value, "no"))
        {
            return false;
        }
        return std::nullopt;
    }

    std::optional<int> ParseLogLevel(std::string_view a_value)
    {
        // Matches spdlog::level::level_enum's underlying values: trace=0,
        // debug=1, info=2, warn=3, err=4. Kept as int literals so ParseIni
        // stays free of any spdlog dependency.
        if (EqualsCaseInsensitive(a_value, "trace"))
        {
            return 0;
        }
        if (EqualsCaseInsensitive(a_value, "debug"))
        {
            return 1;
        }
        if (EqualsCaseInsensitive(a_value, "info"))
        {
            return 2;
        }
        if (EqualsCaseInsensitive(a_value, "warn"))
        {
            return 3;
        }
        if (EqualsCaseInsensitive(a_value, "error"))
        {
            return 4;
        }
        return std::nullopt;
    }

    void WarnBadValue(const char* a_key)
    {
#ifndef MERIDIAN_INICONFIG_NO_RUNTIME
        spdlog::warn("MeridianUI.ini: malformed value for \"{}\"; ignoring", a_key);
#else
        (void)a_key;
#endif
    }
}

namespace Meridian::Config
{
    IniOverrides ParseIni(const char* a_iniText)
    {
        IniOverrides overrides{};

        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadData(a_iniText == nullptr ? "" : a_iniText) < 0)
        {
            return overrides;
        }

        if (const char* value = ini.GetValue("General", "RendererType", nullptr))
        {
            if (EqualsCaseInsensitive(value, "RingBuffer"))
            {
                overrides.rendererType = Meridian::UI::RendererType::RingBuffer;
            }
            else if (EqualsCaseInsensitive(value, "SyncCopy"))
            {
                overrides.rendererType = Meridian::UI::RendererType::SyncCopy;
            }
            else
            {
                WarnBadValue("General.RendererType");
            }
        }

        if (const char* value = ini.GetValue("General", "NativeMenuLangSwitching", nullptr))
        {
            if (const auto parsed = ParseBool(value))
            {
                overrides.nativeMenuLangSwitching = *parsed;
            }
            else
            {
                WarnBadValue("General.NativeMenuLangSwitching");
            }
        }

        if (const char* value = ini.GetValue("General", "AllowRemoteContent", nullptr))
        {
            if (const auto parsed = ParseBool(value))
            {
                overrides.allowRemoteContent = *parsed;
            }
            else
            {
                WarnBadValue("General.AllowRemoteContent");
            }
        }

        if (const char* value = ini.GetValue("Debug", "RemoteDebuggingEnabled", nullptr))
        {
            if (const auto parsed = ParseBool(value))
            {
                overrides.remoteDebuggingEnabled = *parsed;
            }
            else
            {
                WarnBadValue("Debug.RemoteDebuggingEnabled");
            }
        }

        if (const char* value = ini.GetValue("Debug", "RemoteDebuggingPort", nullptr))
        {
            const auto valueLen = std::strlen(value);
            int port = 0;
            const auto result = std::from_chars(value, value + valueLen, port);
            if (result.ec == std::errc{} && result.ptr == value + valueLen && port >= 0 && port <= 65535)
            {
                overrides.remoteDebuggingPort = port;
            }
            else
            {
                WarnBadValue("Debug.RemoteDebuggingPort");
            }
        }

        if (const char* value = ini.GetValue("Debug", "LogLevel", nullptr))
        {
            if (const auto parsed = ParseLogLevel(value))
            {
                overrides.logLevel = *parsed;
            }
            else
            {
                WarnBadValue("Debug.LogLevel");
            }
        }

        return overrides;
    }

#ifndef MERIDIAN_INICONFIG_NO_RUNTIME
    const IniOverrides& LoadIniOverrides()
    {
        static const IniOverrides overrides = [] {
            const auto iniPath = Meridian::Paths::SksePluginsRoot(Meridian::Paths::GameRoot()) / "MeridianUI.ini";

            std::error_code ec;
            if (!std::filesystem::exists(iniPath, ec) || ec)
            {
                return IniOverrides{};
            }

            std::ifstream file(iniPath, std::ios::binary);
            if (!file)
            {
                return IniOverrides{};
            }

            std::ostringstream buffer;
            buffer << file.rdbuf();

            spdlog::info("MeridianUI: loaded config overrides from \"{}\"", iniPath.string());
            return ParseIni(buffer.str().c_str());
        }();
        return overrides;
    }
#endif

    void ApplyOverrides(const IniOverrides& a_overrides, Meridian::UI::Settings& a_settings)
    {
        if (a_overrides.rendererType)
        {
            a_settings.rendererType = *a_overrides.rendererType;
        }
        if (a_overrides.remoteDebuggingEnabled)
        {
            if (!*a_overrides.remoteDebuggingEnabled)
            {
                a_settings.remoteDebuggingPort = 0;
            }
            else if (a_overrides.remoteDebuggingPort)
            {
                a_settings.remoteDebuggingPort = *a_overrides.remoteDebuggingPort;
            }
        }
        if (a_overrides.nativeMenuLangSwitching)
        {
            a_settings.nativeMenuLangSwitching = *a_overrides.nativeMenuLangSwitching;
        }
        if (a_overrides.allowRemoteContent)
        {
            a_settings.allowRemoteContent = *a_overrides.allowRemoteContent;
        }
    }
}
