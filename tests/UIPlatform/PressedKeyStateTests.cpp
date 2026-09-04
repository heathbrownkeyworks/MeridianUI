#include "Common/PressedKeyState.h"

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

    using State = Meridian::Common::PressedKeyState;

    void TestDrainBalancesRegularKeysBeforeModifiers()
    {
        State state;
        state.Press(0x2A, 0x10, true);   // left shift
        state.Press(0x18, 'O', false);  // O

        const auto drained = state.Drain();
        Expect(drained == std::vector<State::Key>{{0x18, 'O', false}, {0x2A, 0x10, true}},
               "focus loss releases ordinary keys before held modifiers");
        Expect(state.Empty(), "drain clears held keys for the next focus session");
    }

    void TestPhysicalReleaseIsNotEmittedTwice()
    {
        State state;
        state.Press(0x18, 'O', false);
        state.Release(0x18);

        Expect(state.Drain().empty(), "a physical key-up removes the key from focus-loss cleanup");
    }

    void TestLockKeyIsDrainedAfterOrdinaryKey()
    {
        State state;
        state.Press(0x3A, 0x14, true);   // caps lock
        state.Press(0x18, 'O', false);  // O

        const auto drained = state.Drain();
        Expect(drained == std::vector<State::Key>{{0x18, 'O', false}, {0x3A, 0x14, true}},
               "focus loss balances Caps Lock after the ordinary key it modified");
    }

    void TestDuplicateDownTracksOneRelease()
    {
        State state;
        state.Press(0x18, 'O', false);
        state.Press(0x18, 'O', false);

        const auto drained = state.Drain();
        Expect(drained.size() == 1 && drained.front().scanCode == 0x18,
               "key repeat still produces one balancing key-up");
    }

    void TestOutOfRangeScanCodeIsIgnored()
    {
        State state;
        state.Press(State::kKeyCount, 'O', false);
        Expect(state.Empty(), "out-of-range scan codes cannot corrupt the held-key ledger");
    }
}

int main()
{
    TestDrainBalancesRegularKeysBeforeModifiers();
    TestPhysicalReleaseIsNotEmittedTwice();
    TestLockKeyIsDrainedAfterOrdinaryKey();
    TestDuplicateDownTracksOneRelease();
    TestOutOfRangeScanCodeIsIgnored();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " pressed-key state test(s) failed\n";
        return 1;
    }

    std::cout << "All pressed-key state tests passed\n";
    return 0;
}
