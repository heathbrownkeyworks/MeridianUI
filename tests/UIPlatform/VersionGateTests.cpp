#include "MeridianUIAPI/Version.h"

#include <cstdint>
#include <iostream>

namespace
{
    std::uint32_t Encode(std::uint32_t a_major, std::uint32_t a_minor)
    {
        return a_major * 100000 + a_minor;
    }

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
    using Meridian::UI::APIVersion::IsCompatible;

    // The original tests parameterized (ourMajor, ourMinor) via a file-local
    // mirror of the gate. IsCompatible is compiled against the real
    // MAJOR/MINOR (1, 0), so only cases valid against that fixed pair are
    // kept; cases like (1, 50) accepted against a hypothetical (1, 1) are
    // dropped rather than re-expressed.
    Expect(!IsCompatible(Encode(2, 0)), "a different major version must be rejected");
    Expect(!IsCompatible(Encode(0, 9)), "an older major version must be rejected");
    Expect(!IsCompatible(Encode(1, 7)), "a newer minor version must be rejected");
    Expect(!IsCompatible(Encode(1, 1)), "any newer minor version must be rejected");
    Expect(IsCompatible(Encode(1, 0)), "the exact current version must be accepted");
    // When MINOR grows past 0, add: IsCompatible(Encode(1, MINOR - 1)) accepted.

    // GetMajorVersion/GetMinorVersion stay live production functions (used by
    // main.cpp's log lines), so cover their round trip through Encode directly.
    Expect(Meridian::UI::APIVersion::GetMajorVersion(Encode(3, 42)) == 3, "major survives the round trip");
    Expect(Meridian::UI::APIVersion::GetMinorVersion(Encode(3, 42)) == 42, "minor survives the round trip");
    Expect(Meridian::UI::APIVersion::GetMinorVersion(Encode(1, 0)) == 0, "zero minor survives the round trip");

    return g_failureCount == 0 ? 0 : 1;
}
