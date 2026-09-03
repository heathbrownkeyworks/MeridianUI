#include "Hooks/CallSiteValidation.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>

namespace
{
    using Meridian::Hooks::CallEncoding;
    using Meridian::Hooks::MatchesExpectedCall;

    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestRelativeCall()
    {
        constexpr std::array<std::uint8_t, 5> call{0xE8, 0x12, 0x34, 0x56, 0x78};
        constexpr std::array<std::uint8_t, 5> jump{0xE9, 0x12, 0x34, 0x56, 0x78};
        constexpr std::array<std::uint8_t, 4> shortCall{0xE8, 0x12, 0x34, 0x56};

        Expect(MatchesExpectedCall(call, CallEncoding::Relative5),
               "E8 rel32 is accepted for a five-byte call hook");
        Expect(!MatchesExpectedCall(jump, CallEncoding::Relative5),
               "E9 rel32 is rejected for a five-byte call hook");
        Expect(!MatchesExpectedCall(shortCall, CallEncoding::Relative5),
               "a truncated relative call is rejected");
    }

    void TestIndirectCall()
    {
        constexpr std::array<std::uint8_t, 6> call{0xFF, 0x15, 0x12, 0x34, 0x56, 0x78};
        constexpr std::array<std::uint8_t, 6> jump{0xFF, 0x25, 0x12, 0x34, 0x56, 0x78};
        constexpr std::array<std::uint8_t, 1> shortCall{0xFF};

        Expect(MatchesExpectedCall(call, CallEncoding::RipRelativeIndirect6),
               "FF /2 RIP-relative call is accepted for a six-byte call hook");
        Expect(!MatchesExpectedCall(jump, CallEncoding::RipRelativeIndirect6),
               "FF /4 RIP-relative jump is rejected for a six-byte call hook");
        Expect(!MatchesExpectedCall(shortCall, CallEncoding::RipRelativeIndirect6),
               "a truncated indirect call is rejected");
    }
}

int main()
{
    TestRelativeCall();
    TestIndirectCall();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " call-site validation test(s) failed\n";
        return 1;
    }

    std::cout << "All call-site validation tests passed\n";
    return 0;
}
