#include "MeridianUIAPI/ViewAPI.h"

#include <iostream>
#include <type_traits>

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
    using namespace Meridian::UI::View;

    static_assert(std::is_standard_layout_v<ViewCreateInfo>);
    static_assert(std::is_same_v<std::underlying_type_t<FocusMode>, std::uint32_t>);
    static_assert(std::is_same_v<std::underlying_type_t<FocusResult>, std::uint32_t>);
    static_assert(INVALID_VIEW_HANDLE == 0);

    ViewCreateInfo info{};
    Expect(info.structSize == sizeof(ViewCreateInfo), "view create info advertises its compiled size");
    Expect(info.frameRate == 60, "view create info defaults to 60 FPS");
    Expect(!info.initiallyVisible, "views default hidden");

    Expect(IsSupported("Meridian.View", 1), "exact Meridian.View/1 query is supported");
    Expect(!IsSupported("Meridian.View", 2), "future interface versions are rejected");
    Expect(!IsSupported("meridian.view", 1), "extension name matching is exact");
    Expect(!IsSupported(nullptr, 1), "null extension names are rejected");

    Expect(static_cast<std::uint32_t>(FocusResult::Granted) == 0, "Granted ABI value is stable");
    Expect(static_cast<std::uint32_t>(FocusResult::Busy) == 2, "Busy ABI value is stable");
    Expect(static_cast<std::uint32_t>(FocusMode::PauseGame) == 1, "PauseGame ABI value is stable");

    return g_failures == 0 ? 0 : 1;
}
