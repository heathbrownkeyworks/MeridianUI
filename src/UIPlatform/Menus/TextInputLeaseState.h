#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

namespace Meridian::Menus
{
    /// Pure synchronized state for Meridian's single ControlMap text-input
    /// lease. Browser/DOM updates change the desired state immediately, while
    /// the game task queue applies only the latest generation. This prevents a
    /// rapid focus/blur pair from releasing a lease that was never acquired.
    class TextInputLeaseState
    {
    public:
        enum class Transition
        {
            Acquire,
            Release
        };

        [[nodiscard]] std::optional<std::uint64_t> SetDesired(bool a_desired)
        {
            std::lock_guard lock(m_mutex);
            if (m_desired == a_desired)
            {
                return std::nullopt;
            }

            m_desired = a_desired;
            return ++m_generation;
        }

        [[nodiscard]] std::optional<Transition> ApplyIfCurrent(std::uint64_t a_generation)
        {
            std::lock_guard lock(m_mutex);
            if (a_generation != m_generation || m_applied == m_desired)
            {
                return std::nullopt;
            }

            m_applied = m_desired;
            return m_applied ? Transition::Acquire : Transition::Release;
        }

        void RejectAcquire()
        {
            std::lock_guard lock(m_mutex);
            m_applied = false;
        }

        [[nodiscard]] bool IsDesired() const
        {
            std::lock_guard lock(m_mutex);
            return m_desired;
        }

        [[nodiscard]] bool IsApplied() const
        {
            std::lock_guard lock(m_mutex);
            return m_applied;
        }

    private:
        mutable std::mutex m_mutex;
        std::uint64_t m_generation = 0;
        bool m_desired = false;
        bool m_applied = false;
    };
}
