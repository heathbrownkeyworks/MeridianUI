#include "NifTestHotkey.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
    int failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    using namespace Meridian::NifTest;

    std::array<std::uint8_t, 256> state{};
    state[KEY_LEFT_ALT] = 0x80;
    Expect(IsToggleHotkey(KEY_N, true, state.data(), state.size()),
           "left Alt+N toggles on the N down edge");

    state.fill(0);
    state[KEY_RIGHT_ALT] = 0x80;
    Expect(IsToggleHotkey(KEY_N, true, state.data(), state.size()),
           "right Alt+N toggles on the N down edge");
    Expect(!IsToggleHotkey(KEY_N, false, state.data(), state.size()),
           "N release does not toggle");
    Expect(!IsToggleHotkey(KEY_LEFT_ALT, true, state.data(), state.size()),
           "pressing Alt while N is held does not toggle");

    state.fill(0);
    Expect(!IsToggleHotkey(KEY_N, true, state.data(), state.size()),
           "N alone does not toggle");
    Expect(!IsToggleHotkey(KEY_N, true, nullptr, state.size()),
           "missing keyboard state does not toggle");

    if (failures != 0)
    {
        std::cerr << failures << " NIF test hotkey test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All NIF test hotkey tests passed\n";
    return EXIT_SUCCESS;
}
