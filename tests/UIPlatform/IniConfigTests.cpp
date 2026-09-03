#include "Config/IniConfig.h"

#include <iostream>

namespace
{
    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    // Full INI parses.
    {
        const auto o = Meridian::Config::ParseIni(
            "[General]\nRendererType = synccopy\nNativeMenuLangSwitching = false\nAllowRemoteContent = true\n"
            "[Debug]\nRemoteDebuggingEnabled = true\nRemoteDebuggingPort = 9111\nLogLevel = debug\n");
        Expect(o.rendererType.has_value() && *o.rendererType == Meridian::UI::RendererType::SyncCopy, "renderer parsed case-insensitively");
        Expect(o.nativeMenuLangSwitching.has_value() && !*o.nativeMenuLangSwitching, "bool parsed");
        Expect(o.allowRemoteContent.has_value() && *o.allowRemoteContent, "remote-content development opt-in parsed");
        Expect(o.remoteDebuggingPort.has_value() && *o.remoteDebuggingPort == 9111, "port parsed");
        Expect(o.logLevel.has_value(), "log level parsed");
    }
    // Empty text = all nullopt.
    {
        const auto o = Meridian::Config::ParseIni("");
        Expect(!o.rendererType && !o.allowRemoteContent && !o.remoteDebuggingEnabled && !o.remoteDebuggingPort && !o.nativeMenuLangSwitching && !o.logLevel, "empty ini overrides nothing");
    }
    // Malformed values stay nullopt.
    {
        const auto o = Meridian::Config::ParseIni("[General]\nRendererType = Turbo\n[Debug]\nRemoteDebuggingPort = fast\n");
        Expect(!o.rendererType, "unknown renderer name ignored");
        Expect(!o.remoteDebuggingPort, "non-numeric port ignored");
    }
    // Numeric but out-of-range port stays nullopt.
    {
        const auto o = Meridian::Config::ParseIni("[Debug]\nRemoteDebuggingPort = 99999\n");
        Expect(!o.remoteDebuggingPort, "out-of-range port ignored");
    }
    // ApplyOverrides: present wins, absent falls through.
    {
        Meridian::UI::Settings s{};
        s.remoteDebuggingPort = 9009;
        Meridian::Config::IniOverrides o{};
        o.rendererType = Meridian::UI::RendererType::SyncCopy;
        o.allowRemoteContent = true;
        Meridian::Config::ApplyOverrides(o, s);
        Expect(s.rendererType == Meridian::UI::RendererType::SyncCopy, "present override wins");
        Expect(s.allowRemoteContent, "remote-content opt-in overrides the secure default");
        Expect(s.remoteDebuggingPort == 9009, "absent override preserves an explicit consumer opt-in");
    }
    // RemoteDebuggingEnabled=false disables regardless of consumer port.
    {
        Meridian::UI::Settings s{};
        Meridian::Config::IniOverrides o{};
        o.remoteDebuggingEnabled = false;
        Meridian::Config::ApplyOverrides(o, s);
        Expect(s.remoteDebuggingPort == 0, "debugging disabled maps to port 0");
    }
    // INI port alone is not a development opt-in; the master switch is required.
    {
        Meridian::UI::Settings s{};
        Meridian::Config::IniOverrides o{};
        o.remoteDebuggingPort = 9009;
        Meridian::Config::ApplyOverrides(o, s);
        Expect(s.remoteDebuggingPort == 0, "port-only INI does not enable remote debugging");

        o.remoteDebuggingEnabled = true;
        Meridian::Config::ApplyOverrides(o, s);
        Expect(s.remoteDebuggingPort == 9009, "enabled plus port explicitly opts into remote debugging");
    }

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " IniConfig test(s) failed\n";
        return 1;
    }

    std::cout << "All IniConfig tests passed\n";
    return 0;
}
