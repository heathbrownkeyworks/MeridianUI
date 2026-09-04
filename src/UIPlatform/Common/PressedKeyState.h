#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Meridian::Common
{
    /// Pure pressed-key ledger used to balance Chromium input when browser
    /// focus ends between a physical key-down and key-up.
    class PressedKeyState
    {
    public:
        static constexpr std::size_t kKeyCount = 256;

        struct Key
        {
            std::uint32_t scanCode = 0;
            std::uint32_t virtualKey = 0;
            bool modifier = false;

            bool operator==(const Key&) const = default;
        };

        void Press(std::uint32_t a_scanCode,
                   std::uint32_t a_virtualKey,
                   bool a_modifier)
        {
            if (a_scanCode >= m_keys.size())
            {
                return;
            }
            m_keys[a_scanCode] = Key{a_scanCode, a_virtualKey, a_modifier};
        }

        void Release(std::uint32_t a_scanCode)
        {
            if (a_scanCode < m_keys.size())
            {
                m_keys[a_scanCode].active = false;
            }
        }

        [[nodiscard]] std::vector<Key> Drain()
        {
            std::vector<Key> result;
            for (const bool modifiers : {false, true})
            {
                for (auto& entry : m_keys)
                {
                    if (entry.active && entry.key.modifier == modifiers)
                    {
                        result.push_back(entry.key);
                        entry.active = false;
                    }
                }
            }
            return result;
        }

        void Clear()
        {
            for (auto& entry : m_keys)
            {
                entry.active = false;
            }
        }

        [[nodiscard]] bool Empty() const
        {
            for (const auto& entry : m_keys)
            {
                if (entry.active)
                {
                    return false;
                }
            }
            return true;
        }

    private:
        struct Entry
        {
            Key key;
            bool active = false;

            Entry& operator=(const Key& a_key)
            {
                key = a_key;
                active = true;
                return *this;
            }
        };

        std::array<Entry, kKeyCount> m_keys{};
    };
}
