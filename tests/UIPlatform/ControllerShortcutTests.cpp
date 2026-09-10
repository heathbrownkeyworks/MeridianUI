#include "Input/InputPolicy.h"
using namespace Meridian::UI::Input;
void __cdecl Callback(ShortcutHandle, void*)
{
}
int main()
{
    ShortcutInfo a;
    a.button = Control::South;
    a.modifier = Control::LeftShoulder;
    a.callback = Callback;
    if (!Meridian::Input::ValidShortcut(a))
        return 1;
    auto b = a;
    if (!Meridian::Input::ShortcutConflict(a, b))
        return 2;
    b.button = Control::East;
    if (Meridian::Input::ShortcutConflict(a, b))
        return 3;
    b.modifier = Control::None;
    b.button = Control::LeftShoulder;
    if (!Meridian::Input::ShortcutConflict(a, b))
        return 4;
    b.button = Control::LeftTrigger;
    if (Meridian::Input::ValidShortcut(b))
        return 5;
    b = a;
    b.modifier = b.button;
    if (Meridian::Input::ValidShortcut(b))
        return 6;
    b = a;
    b.reserved = 1;
    if (Meridian::Input::ValidShortcut(b))
        return 7;
    return 0;
}
