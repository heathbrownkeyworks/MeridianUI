#pragma once
#include "MeridianUIAPI/InputAPI.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace Meridian::Input
{
    using UI::Input::Control;
    using UI::Input::Action;
    using UI::Input::Mode;
    inline constexpr std::size_t ControlCount = 19;
    enum class Phase
    {
        None,
        Press,
        Release,
        Change,
        Repeat,
        Cancel
    };
    struct Transition
    {
        bool consume = false;
        Phase phase = Phase::None;
    };
    inline float ClampAxis(float value)
    {
        return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    }
    inline constexpr bool IsKeyboardDevice(bool keyboard, bool virtualKeyboard)
    {
        return keyboard || virtualKeyboard;
    }
    inline constexpr bool IsDigital(Control c)
    {
        return c >= Control::DpadUp && c <= Control::North;
    }
    inline constexpr std::size_t Index(Control c)
    {
        return static_cast<std::size_t>(c);
    }
    inline constexpr Control FromSKSEKeycode(std::uint32_t id)
    {
        return id >= 266 && id <= 281 ? static_cast<Control>(id - 265) : Control::None;
    }
    inline constexpr Control FromXInputButton(std::uint32_t id)
    {
        constexpr std::uint32_t ids[]{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 4096, 8192, 16384, 32768, 9, 10};
        for (std::size_t i = 0; i < 16; ++i)
            if (ids[i] == id)
                return static_cast<Control>(i + 1);
        return Control::None;
    }
    inline constexpr const char* ControlName(Control c)
    {
        constexpr const char* names[]{"none", "dpadUp", "dpadDown", "dpadLeft", "dpadRight", "start", "back", "leftThumb", "rightThumb", "leftShoulder", "rightShoulder", "south", "east", "west", "north", "leftTrigger", "rightTrigger", "leftStick", "rightStick"};
        return Index(c) < ControlCount ? names[Index(c)] : "none";
    }
    inline constexpr const char* ActionName(Action a)
    {
        constexpr const char* names[]{"none", "up", "down", "left", "right", "accept", "cancel", "previousTab", "nextTab", "secondary", "tertiary", "toggleCursor"};
        auto i = static_cast<unsigned>(a);
        return i < std::size(names) ? names[i] : "none";
    }
    inline constexpr const char* PhaseName(Phase p)
    {
        switch (p)
        {
        case Phase::Press:
            return "press";
        case Phase::Release:
            return "release";
        case Phase::Change:
            return "change";
        case Phase::Repeat:
            return "repeat";
        case Phase::Cancel:
            return "cancel";
        default:
            return "none";
        }
    }
    struct ControllerTuning
    {
        bool enabled = true;
        bool trace = false;
        float deadZone = 0.25f;
        float exitDeadZone = 0.18f;
        float cursorSpeed = 900.0f;
        double repeatDelay = 0.350;
        double repeatInterval = 0.090;
        UI::Input::GlyphFamily glyphFamily = UI::Input::GlyphFamily::Xbox;
    };
}
