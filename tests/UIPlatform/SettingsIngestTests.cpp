#include "MeridianUIAPI/Settings.h"
#include "MeridianUIAPI/SettingsIngest.h"

#include <cstdint>
#include <cstring>
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
    // Exact-size caller: all fields land.
    {
        Meridian::UI::Settings in{};
        in.remoteDebuggingPort = 1234;
        in.rendererType = Meridian::UI::RendererType::SyncCopy;
        Meridian::UI::Settings out{};
        Expect(Meridian::UI::IngestSettings(&in, sizeof(Meridian::UI::Settings), out), "exact-size caller accepted");
        Expect(out.remoteDebuggingPort == 1234 && out.rendererType == Meridian::UI::RendererType::SyncCopy, "fields copied");
        Expect(out.structSize == sizeof(Meridian::UI::Settings), "structSize normalized to ours");
    }
    // Forward-compat caller (future us, running against a newer consumer
    // whose Settings grew fields we don't know about yet): structSize claims
    // MORE than sizeof(Settings). IngestSettings must accept it, copy only
    // sizeof(Settings) bytes (never read past our own layout), normalize
    // structSize to ours, and land the real fields from the known prefix.
    // This is the callerSize > sizeof(TSettings) clamp branch in
    // IngestSettings — previously untested.
    {
        constexpr std::uint32_t futureSize = sizeof(Meridian::UI::Settings) + 8;
        unsigned char buffer[futureSize] = {};
        std::memcpy(buffer, &futureSize, sizeof(futureSize));
        const int port = 7777;
        std::memcpy(buffer + offsetof(Meridian::UI::Settings, remoteDebuggingPort), &port, sizeof(port));
        const auto rendererType = Meridian::UI::RendererType::SyncCopy;
        std::memcpy(buffer + offsetof(Meridian::UI::Settings, rendererType), &rendererType, sizeof(rendererType));
        Meridian::UI::Settings out{};
        Expect(Meridian::UI::IngestSettings(buffer, Meridian::UI::kSettingsMinSize10, out), "future-larger caller accepted");
        Expect(out.remoteDebuggingPort == 7777 && out.rendererType == Meridian::UI::RendererType::SyncCopy, "known-prefix fields land");
        Expect(out.structSize == sizeof(Meridian::UI::Settings), "structSize normalized to ours, not the caller's larger claim");
    }
    // Pre-1.0 caller (structSize below the 1.0 minimum) refused. At
    // kSettingsMinSize10 == sizeof(Settings) there is no valid
    // smaller-than-ours 1.0 caller — anything below the minimum is refused.
    {
        unsigned char buffer[16] = {};
        const std::uint32_t claimed = 8;
        std::memcpy(buffer, &claimed, sizeof(claimed));
        Meridian::UI::Settings out{};
        Expect(!Meridian::UI::IngestSettings(buffer, Meridian::UI::kSettingsMinSize10, out), "below-minimum caller refused");
    }
    // Garbage size refused.
    {
        unsigned char buffer[16] = {};
        const std::uint32_t claimed = 0xFFFFFFFF;
        std::memcpy(buffer, &claimed, sizeof(claimed));
        Meridian::UI::Settings out{};
        Expect(!Meridian::UI::IngestSettings(buffer, Meridian::UI::kSettingsMinSize10, out), "garbage size refused");
    }
    // Null caller = defaults, accepted.
    {
        Meridian::UI::Settings out{};
        out.remoteDebuggingPort = -1;
        Expect(Meridian::UI::IngestSettings(nullptr, Meridian::UI::kSettingsMinSize10, out), "null caller accepted");
        Expect(out.remoteDebuggingPort == 0, "null caller yields secure defaults");
    }

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " SettingsIngest test(s) failed\n";
        return 1;
    }

    std::cout << "All SettingsIngest tests passed\n";
    return 0;
}
