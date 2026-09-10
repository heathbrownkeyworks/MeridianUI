#include "MeridianUIAPI/InputAPI.h"
#include "Input/InputPolicy.h"
#include <type_traits>
using namespace Meridian::UI::Input;
int main()
{
    static_assert(std::is_standard_layout_v<ViewInputConfig>);
    static_assert(std::is_standard_layout_v<InputState>);
    static_assert(std::is_standard_layout_v<ShortcutInfo>);
    static_assert(sizeof(Control) == 4 && sizeof(Action) == 4);
    static_assert(sizeof(ViewInputConfig) == 72 && sizeof(InputState) == 48 && sizeof(ShortcutInfo) == 32);
    static_assert(offsetof(InputState, generation) == 24 && offsetof(ShortcutInfo, callback) == 16);
    if (!IsSupported("Meridian.Input", 1) || IsSupported("Meridian.Input", 2) || IsSupported(nullptr, 1))
        return 1;
    ViewInputConfig config;
    if (config.enabled != 0 || !Meridian::Input::ValidConfig(config))
        return 2;
    config.bindings[0] = Control::East;
    if (Meridian::Input::ValidConfig(config))
        return 3;
    config = {};
    config.structSize = 0;
    if (Meridian::Input::ValidConfig(config))
        return 4;
    config = {};
    config.allowCursor = 2;
    if (Meridian::Input::ValidConfig(config))
        return 5;
    return 0;
}
