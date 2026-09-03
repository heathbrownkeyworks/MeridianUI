#include "Menus/CursorPolicy.h"

#include <iostream>

int main()
{
    const auto unfocused = Meridian::Menus::CursorPolicy::Evaluate(false);
    if (unfocused.drawMeridianCursor || unfocused.hideVanillaCursor)
    {
        std::cerr << "FAILED: an unfocused Meridian view must not change cursor ownership\n";
        return 1;
    }

    const auto focused = Meridian::Menus::CursorPolicy::Evaluate(true);
    if (!focused.drawMeridianCursor || !focused.hideVanillaCursor)
    {
        std::cerr << "FAILED: a focused Meridian view must own the final cursor\n";
        return 1;
    }

    std::cout << "All CursorPolicy tests passed\n";
    return 0;
}
