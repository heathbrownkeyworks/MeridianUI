#pragma once
#include "ControllerTypes.h"
namespace Meridian::Input
{
    inline bool ValidConfig(const UI::Input::ViewInputConfig& c)
    {
        if (c.structSize < sizeof(c) || c.enabled > 1 || c.allowCursor > 1 ||
            c.defaultMode > Mode::Cursor || (!c.allowCursor && c.defaultMode == Mode::Cursor))
            return false;
        for (auto r : c.reserved)
            if (r)
                return false;
        for (unsigned i = 0; i < UI::Input::BINDING_COUNT; ++i)
        {
            if (c.bindings[i] != Control::None && !IsDigital(c.bindings[i]))
                return false;
            for (unsigned j = 0; j < i; ++j)
                if (c.bindings[i] != Control::None && c.bindings[j] == c.bindings[i])
                    return false;
        }
        return true;
    }
    inline bool ValidShortcut(const UI::Input::ShortcutInfo& s)
    {
        return s.structSize >= sizeof(s) && s.reserved == 0 && s.callback && IsDigital(s.button) &&
               (s.modifier == Control::None || IsDigital(s.modifier)) && s.button != s.modifier;
    }
    inline bool ShortcutConflict(const UI::Input::ShortcutInfo& a, const UI::Input::ShortcutInfo& b)
    {
        if (a.button == b.button)
            return true;
        if (a.modifier == Control::None)
            return a.button == b.modifier;
        if (b.modifier == Control::None)
            return b.button == a.modifier;
        return a.button == b.modifier && a.modifier == b.button;
    }
    inline Action BindingAction(const UI::Input::ViewInputConfig& c, Control control)
    {
        for (unsigned i = 0; i < UI::Input::BINDING_COUNT; ++i)
            if (control != Control::None && c.bindings[i] == control)
                return static_cast<Action>(i + 1);
        return Action::None;
    }
}
