#include "Menus/FocusSessionState.h"

#include <array>
#include <iostream>

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
}

int main()
{
    Meridian::Menus::FocusSessionState state;
    std::array<std::uint8_t, Meridian::Menus::FocusSessionState::kKeyboardStateSize> keyboard{};
    keyboard[0x2A] = 0x80;

    const auto firstGeneration = state.Begin(false, keyboard.data(), keyboard.size());
    Expect(state.IsOpeningKeyHeld(0x2A), "opening modifier is captured");
    Expect(!state.IsOpeningKeyHeld(0x1D), "unheld key is not captured");
    Expect(state.ConsumeOpeningKeyRelease(0x2A), "opening modifier release is consumed once");
    Expect(!state.ConsumeOpeningKeyRelease(0x2A), "opening modifier release is not consumed twice");
    state.End();

    const auto firstRestore = state.TakePendingRestore();
    Expect(firstRestore.has_value() && firstRestore->generation == firstGeneration,
           "ended focus session produces its generation ticket");
    Expect(firstRestore.has_value() && firstRestore->running.has_value() && !*firstRestore->running,
           "walk mode is preserved exactly");
    Expect(!state.TakePendingRestore().has_value(), "restore ticket is one-shot");
    Expect(state.IsRestoreCurrent(firstGeneration), "completed generation remains current until a new focus session");

    const auto staleGeneration = state.Begin(true, nullptr, 0);
    state.End();
    const auto replacementGeneration = state.Begin(false, nullptr, 0);
    Expect(replacementGeneration > staleGeneration, "each focus session receives a newer generation");
    Expect(!state.TakePendingRestore().has_value(), "new focus invalidates an undispatched stale restore");
    Expect(!state.IsRestoreCurrent(staleGeneration), "old restore cannot apply during a newer focus session");
    state.End();

    const auto finalRestore = state.TakePendingRestore();
    Expect(finalRestore.has_value() && finalRestore->generation == replacementGeneration,
           "latest focus session owns the restore ticket");
    Expect(finalRestore.has_value() && finalRestore->running.has_value() && !*finalRestore->running,
           "latest exact run state is retained");
    Expect(state.IsRestoreCurrent(replacementGeneration), "latest completed restore is current");

    const auto unknownGeneration = state.Begin(std::nullopt, nullptr, 0);
    state.End();
    const auto unknownRestore = state.TakePendingRestore();
    Expect(unknownRestore.has_value() && unknownRestore->generation == unknownGeneration &&
               !unknownRestore->running.has_value(),
           "unavailable player state is represented without inventing a value");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " focus-session test(s) failed\n";
        return 1;
    }
    std::cout << "All focus-session tests passed\n";
    return 0;
}
