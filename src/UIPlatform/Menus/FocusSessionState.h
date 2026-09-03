#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Meridian::Menus
{
    /// Pure, thread-safe state for one platform focus session. The runtime
    /// captures player/input state at the first claim, ends the session at the
    /// last release, and consumes the restore ticket only after FocusMenu has
    /// actually closed.
    class FocusSessionState
    {
    public:
        static constexpr std::size_t kKeyboardStateSize = 256;

        struct RestoreTicket
        {
            std::uint64_t generation = 0;
            std::optional<bool> running;
        };

        std::uint64_t Begin(std::optional<bool> a_running,
                            const std::uint8_t* a_keyboardState,
                            std::size_t a_keyboardStateSize)
        {
            std::lock_guard lock(m_mutex);
            ++m_generation;
            m_active = true;
            m_capturedRunning = a_running;
            m_pendingRestore.reset();
            m_openingKeys.fill(false);

            if (a_keyboardState != nullptr)
            {
                const auto count = a_keyboardStateSize < m_openingKeys.size() ?
                    a_keyboardStateSize : m_openingKeys.size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    m_openingKeys[i] = a_keyboardState[i] != 0;
                }
            }
            return m_generation;
        }

        void End()
        {
            std::lock_guard lock(m_mutex);
            if (!m_active)
            {
                return;
            }

            m_active = false;
            m_pendingRestore = RestoreTicket{m_generation, m_capturedRunning};
            m_openingKeys.fill(false);
        }

        [[nodiscard]] std::optional<RestoreTicket> TakePendingRestore()
        {
            std::lock_guard lock(m_mutex);
            if (m_active || !m_pendingRestore.has_value())
            {
                return std::nullopt;
            }

            auto ticket = m_pendingRestore;
            m_pendingRestore.reset();
            return ticket;
        }

        [[nodiscard]] bool IsRestoreCurrent(std::uint64_t a_generation) const
        {
            std::lock_guard lock(m_mutex);
            return !m_active && m_generation == a_generation;
        }

        [[nodiscard]] bool IsOpeningKeyHeld(std::uint32_t a_scanCode) const
        {
            std::lock_guard lock(m_mutex);
            return m_active && a_scanCode < m_openingKeys.size() && m_openingKeys[a_scanCode];
        }

        bool ConsumeOpeningKeyRelease(std::uint32_t a_scanCode)
        {
            std::lock_guard lock(m_mutex);
            if (!m_active || a_scanCode >= m_openingKeys.size() || !m_openingKeys[a_scanCode])
            {
                return false;
            }

            m_openingKeys[a_scanCode] = false;
            return true;
        }

    private:
        mutable std::mutex m_mutex;
        std::uint64_t m_generation = 0;
        bool m_active = false;
        std::optional<bool> m_capturedRunning;
        std::optional<RestoreTicket> m_pendingRestore;
        std::array<bool, kKeyboardStateSize> m_openingKeys{};
    };
}
