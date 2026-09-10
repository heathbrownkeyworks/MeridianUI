#pragma once
#include "ControllerTypes.h"
#include <array>
#include <cstdint>
namespace Meridian::Input
{
    // Engine-independent physical/ownership ledger. The service serializes access.
    class ControllerState
    {
    public:
        ControllerTuning tuning;
        bool SetOwner(std::uint64_t owner)
        {
            if (owner == m_owner)
                return false;
            const bool leavingOwner = m_owner != 0;
            m_owner = owner;
            ++m_generation;
            m_repeatDirection = Control::None;
            for (std::size_t i = 1; i < ControlCount; ++i)
                if (m_value[i] != 0 || m_y[i] != 0)
                {
                    m_blocked[i] = true;
                    if (leavingOwner)
                        m_uiOwned[i] = true;
                }
            return true;
        }
        void RequireNeutral()
        {
            m_blocked.fill(true);
            m_repeatDirection = Control::None;
        }
        void Seed(Control c, float x, float y = 0)
        {
            if (c == Control::None || Index(c) >= ControlCount)
                return;
            auto i = Index(c);
            m_value[i] = ClampAxis(x);
            m_y[i] = ClampAxis(y);
            const bool stick = c == Control::LeftStick || c == Control::RightStick;
            const bool held = stick ? std::hypot(x, y) >= tuning.deadZone : x > 0.1f;
            m_blocked[i] = held;
            if (!held)
                m_value[i] = m_y[i] = 0;
        }
        void ResetDevice()
        {
            m_value.fill(0);
            m_y.fill(0);
            m_gameDown.fill(false);
            m_uiOwned.fill(false);
            RequireNeutral();
            ++m_generation;
        }
        void Capture(Control c)
        {
            if (Index(c) < ControlCount)
                m_blocked[Index(c)] = true;
        }
        bool Held(Control c) const
        {
            return Index(c) < ControlCount && m_value[Index(c)] > 0;
        }
        bool Armed(Control c) const
        {
            return Index(c) < ControlCount && !m_blocked[Index(c)];
        }
        float X(Control c) const
        {
            return Armed(c) ? m_value[Index(c)] : 0;
        }
        float Y(Control c) const
        {
            return Armed(c) ? m_y[Index(c)] : 0;
        }
        std::uint64_t Generation() const
        {
            return m_generation;
        }
        std::uint64_t Owner() const
        {
            return m_owner;
        }
        bool InitialRepeat() const
        {
            return m_initialRepeat;
        }
        Transition Button(Control c, float value, bool forceCapture = false)
        {
            if (c < Control::DpadUp || c > Control::RightTrigger)
                return {};
            auto i = Index(c);
            value = std::clamp(ClampAxis(value), 0.0f, 1.0f);
            if (c == Control::LeftTrigger || c == Control::RightTrigger)
            {
                if (value < 0.10f)
                    value = 0;
            }
            const float old = m_value[i];
            m_value[i] = value;
            bool wasBlocked = m_blocked[i];
            // Rearming UI actions is separate from suppressing gameplay. A
            // reconnect with no UI owner must preserve normal game input.
            bool consumed = m_owner != 0 || m_uiOwned[i] || forceCapture;
            Phase phase = value == old ? Phase::None : (value == 0 ? Phase::Release : (old == 0 ? Phase::Press : Phase::Change));
            if (value == 0)
            {
                // A game-owned down must be paired even when it shares a batch
                // with captured keys. UI never receives its opening release.
                if (m_gameDown[i])
                    consumed = false;
                m_gameDown[i] = false;
                m_blocked[i] = false;
                m_uiOwned[i] = false;
            }
            else if (!consumed)
                m_gameDown[i] = true;
            else
                m_uiOwned[i] = true;
            if (forceCapture && value != 0)
                m_blocked[i] = true;
            if (wasBlocked || forceCapture || !m_owner)
                phase = Phase::None;
            // Pass-through transitions are useful for activity/shortcut tracking.
            if (!m_owner && !wasBlocked && !forceCapture)
                phase = value == old ? Phase::None : (value == 0 ? Phase::Release : old == 0 ? Phase::Press
                                                                                             : Phase::Change);
            return {consumed, phase};
        }
        Transition Stick(Control c, float x, float y)
        {
            if (c != Control::LeftStick && c != Control::RightStick)
                return {};
            auto i = Index(c);
            x = ClampAxis(x);
            y = ClampAxis(y);
            const float threshold = (m_value[i] != 0 || m_y[i] != 0) ? tuning.exitDeadZone : tuning.deadZone;
            if (std::hypot(x, y) < threshold)
                x = y = 0;
            bool changed = x != m_value[i] || y != m_y[i];
            m_value[i] = x;
            m_y[i] = y;
            bool blocked = m_blocked[i];
            bool consume = m_owner != 0 || m_uiOwned[i];
            if (x == 0 && y == 0)
            {
                if (m_gameDown[i])
                    consume = false;
                m_gameDown[i] = false;
                m_blocked[i] = false;
                m_uiOwned[i] = false;
            }
            else if (!consume)
                m_gameDown[i] = true;
            else
                m_uiOwned[i] = true;
            return {consume, changed && m_owner && !blocked ? Phase::Change : Phase::None};
        }
        Control Direction(const UI::Input::ViewInputConfig* config = nullptr) const
        {
            if (!m_owner)
                return Control::None;
            for (unsigned i = 0; i < 4; ++i)
            {
                auto c = config ? config->bindings[i] : static_cast<Control>(i + 1);
                if (c != Control::None && Armed(c) && Held(c))
                    return static_cast<Control>(i + 1);
            }
            const float x = X(Control::LeftStick), y = Y(Control::LeftStick);
            if (x == 0 && y == 0)
                return Control::None;
            // Keep the current axis around diagonals to avoid frame-to-frame chatter.
            const bool wasHorizontal = m_repeatDirection == Control::DpadLeft || m_repeatDirection == Control::DpadRight;
            const bool horizontal = std::abs(x) > std::abs(y) * (wasHorizontal ? 0.85f : 1.15f);
            return horizontal ? (x > 0 ? Control::DpadRight : Control::DpadLeft) : (y > 0 ? Control::DpadUp : Control::DpadDown);
        }
        Control Repeat(double now, bool includeStick = true, const UI::Input::ViewInputConfig* config = nullptr)
        {
            auto direction = Direction(config);
            if (!includeStick)
            {
                direction = Control::None;
                for (unsigned i = 0; i < 4; ++i)
                {
                    auto c = config ? config->bindings[i] : static_cast<Control>(i + 1);
                    if (c != Control::None && Armed(c) && Held(c))
                    {
                        direction = static_cast<Control>(i + 1);
                        break;
                    }
                }
            }
            if (direction != m_repeatDirection)
            {
                m_initialRepeat = true;
                m_repeatDirection = direction;
                m_nextRepeat = now + tuning.repeatDelay;
                return direction;
            }
            if (direction != Control::None && now >= m_nextRepeat)
            {
                m_initialRepeat = false;
                m_nextRepeat = now + tuning.repeatInterval;
                return direction;
            }
            return Control::None;
        }

    private:
        std::uint64_t m_owner = 0, m_generation = 0;
        std::array<float, ControlCount> m_value{}, m_y{};
        std::array<bool, ControlCount> m_blocked{}, m_gameDown{}, m_uiOwned{};
        Control m_repeatDirection = Control::None;
        double m_nextRepeat = 0;
        bool m_initialRepeat = false;
    };
}
