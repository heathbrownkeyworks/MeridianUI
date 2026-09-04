#include "Common/InputDispatchFilter.h"
#include "Common/InputSinkPriority.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    enum class Sink
    {
        Existing,
        Router,
        LanguageSwitch,
        LateExternal,
    };

    void TestLateExternalSinkCannotPrecedeMeridian()
    {
        std::vector<Sink> sinks{
            Sink::LateExternal,
            Sink::Existing,
            Sink::Router,
            Sink::LanguageSwitch,
        };

        Meridian::Common::PromoteInputSinks(sinks, Sink::Router, Sink::LanguageSwitch);

        Expect(sinks == std::vector<Sink>{
                            Sink::LanguageSwitch,
                            Sink::Router,
                            Sink::LateExternal,
                            Sink::Existing,
                        },
               "late external sink is placed behind Meridian's internal sinks");
    }

    void TestMissingLanguageSinkStillPromotesRouter()
    {
        std::vector<Sink> sinks{Sink::LateExternal, Sink::Existing, Sink::Router};
        Meridian::Common::PromoteInputSinks(sinks, Sink::Router, Sink::LanguageSwitch);

        Expect(sinks == std::vector<Sink>{Sink::Router, Sink::LateExternal, Sink::Existing},
               "router is first when language switching is inactive");
    }

    void TestFocusedStopPreventsExternalHotkey()
    {
        std::vector<Sink> sinks{Sink::LateExternal, Sink::Router};
        Meridian::Common::PromoteInputSinks(sinks, Sink::Router, Sink::LanguageSwitch);

        bool externalHotkeyFired = false;
        for (const auto sink : sinks)
        {
            if (sink == Sink::Router)
            {
                break;
            }
            if (sink == Sink::LateExternal)
            {
                externalHotkeyFired = true;
            }
        }

        Expect(!externalHotkeyFired, "focused router stops dispatch before a competing hotkey sink");
    }

    void TestUnfocusedContinuePreservesExternalHotkey()
    {
        std::vector<Sink> sinks{Sink::LateExternal, Sink::Router};
        Meridian::Common::PromoteInputSinks(sinks, Sink::Router, Sink::LanguageSwitch);

        bool externalHotkeyFired = false;
        for (const auto sink : sinks)
        {
            if (sink == Sink::LateExternal)
            {
                externalHotkeyFired = true;
            }
        }

        Expect(externalHotkeyFired, "unfocused router preserves normal downstream hotkeys");
    }

    void TestConsumedBatchIsHiddenFromDirectCompetingHook()
    {
        const std::vector<Sink> original{Sink::LanguageSwitch, Sink::LateExternal};
        const std::vector<Sink> empty;

        const auto* forwarded = Meridian::Common::SelectForwardedInput(
            true,
            &original,
            &empty);

        const bool externalHotkeyFired =
            std::find(forwarded->begin(), forwarded->end(), Sink::LateExternal) != forwarded->end();
        Expect(!externalHotkeyFired,
               "a consumed batch is empty before a direct competing hook can inspect it");
    }

    void TestUnconsumedBatchReachesDirectCompetingHook()
    {
        const std::vector<Sink> original{Sink::LanguageSwitch, Sink::LateExternal};
        const std::vector<Sink> empty;

        const auto* forwarded = Meridian::Common::SelectForwardedInput(
            false,
            &original,
            &empty);

        const bool externalHotkeyFired =
            std::find(forwarded->begin(), forwarded->end(), Sink::LateExternal) != forwarded->end();
        Expect(externalHotkeyFired,
               "an unconsumed batch remains visible to direct competing hooks");
    }
}

int main()
{
    TestLateExternalSinkCannotPrecedeMeridian();
    TestMissingLanguageSinkStillPromotesRouter();
    TestFocusedStopPreventsExternalHotkey();
    TestUnfocusedContinuePreservesExternalHotkey();
    TestConsumedBatchIsHiddenFromDirectCompetingHook();
    TestUnconsumedBatchReachesDirectCompetingHook();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " input-sink priority test(s) failed\n";
        return 1;
    }

    std::cout << "All input-sink priority tests passed\n";
    return 0;
}
