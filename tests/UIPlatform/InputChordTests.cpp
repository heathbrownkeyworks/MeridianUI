#include "Common/InputChord.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace
{
    using namespace Meridian::Common;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestBothZeroNeverFires()
    {
        std::uint8_t state[8] = {};
        state[0] = 0x80; // even if index 0 were "held", both-zero must still be unarmed
        Expect(!ChordSatisfied(0, 0, state, sizeof(state)), "both-zero chord never fires, even with state[0] held");
    }

    void TestSingleKeyChord()
    {
        std::uint8_t state[8] = {};
        state[3] = 0x80;
        Expect(ChordSatisfied(3, 0, state, sizeof(state)), "single-key chord fires when held");
        Expect(!ChordSatisfied(4, 0, state, sizeof(state)), "single-key chord does not fire when clear");

        state[3] = 0x00;
        Expect(!ChordSatisfied(3, 0, state, sizeof(state)), "single-key chord does not fire once released");
    }

    void TestTwoKeyChordRequiresBoth()
    {
        std::uint8_t state[8] = {};
        state[1] = 0x80;
        Expect(!ChordSatisfied(1, 2, state, sizeof(state)), "two-key chord does not fire with only one half held");

        state[2] = 0x80;
        Expect(ChordSatisfied(1, 2, state, sizeof(state)), "two-key chord fires once both halves are held");

        state[1] = 0x00;
        Expect(!ChordSatisfied(1, 2, state, sizeof(state)), "two-key chord stops firing once either half releases");
    }

    void TestOutOfRangeCodeDoesNotFireOrReadOob()
    {
        std::uint8_t state[4] = {0x80, 0x80, 0x80, 0x80};
        Expect(!ChordSatisfied(4, 0, state, sizeof(state)), "code equal to state size is out of range and does not fire");
        Expect(!ChordSatisfied(1000, 0, state, sizeof(state)), "far out-of-range code does not fire (and must not read OOB)");
        Expect(!ChordSatisfied(1, 1000, state, sizeof(state)), "out-of-range second half does not fire");
        Expect(!ChordSatisfied(3, 4, state, sizeof(state)), "one in-range, one out-of-range half does not fire");
    }

    void TestNullStateNeverFires()
    {
        Expect(!ChordSatisfied(1, 2, nullptr, 256), "null key state never fires");
        Expect(!ChordSatisfied(0, 0, nullptr, 256), "null key state with both-zero also never fires");
    }
}

int main()
{
    TestBothZeroNeverFires();
    TestSingleKeyChord();
    TestTwoKeyChordRequiresBoth();
    TestOutOfRangeCodeDoesNotFireOrReadOob();
    TestNullStateNeverFires();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " InputChord test(s) failed\n";
        return 1;
    }

    std::cout << "All InputChord tests passed\n";
    return 0;
}
