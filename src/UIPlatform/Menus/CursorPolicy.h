#pragma once

namespace Meridian::Menus
{
    struct CursorDecision
    {
        bool drawMeridianCursor;
        bool hideVanillaCursor;
    };

    class CursorPolicy
    {
    public:
        static constexpr CursorDecision Evaluate(bool a_meridianHasFocus)
        {
            return {
                .drawMeridianCursor = a_meridianHasFocus,
                .hideVanillaCursor = a_meridianHasFocus,
            };
        }
    };
}
