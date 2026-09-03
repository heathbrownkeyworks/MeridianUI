#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Meridian::Render
{
    /// <summary>
    /// Tiny LRU keyed by cursor handle. Bounded so pages cycling custom CSS
    /// cursors can't grow textures without limit. Pure logic — the value
    /// type is a template so tests run without D3D.
    /// </summary>
    template <typename TValue>
    class CursorTextureCache
    {
    public:
        static constexpr std::size_t kCapacity = 8;

        TValue* Get(std::uintptr_t a_key)
        {
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries[i].key == a_key)
                {
                    Touch(i);
                    return &m_entries.back().value;
                }
            }
            return nullptr;
        }

        void Put(std::uintptr_t a_key, TValue a_value)
        {
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries[i].key == a_key)
                {
                    m_entries[i].value = std::move(a_value);
                    Touch(i);
                    return;
                }
            }
            if (m_entries.size() >= kCapacity)
            {
                m_entries.erase(m_entries.begin());  // front = least recently used
            }
            m_entries.push_back({a_key, std::move(a_value)});
        }

        bool Erase(std::uintptr_t a_key)
        {
            for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            {
                if (it->key == a_key)
                {
                    m_entries.erase(it);
                    return true;
                }
            }
            return false;
        }

        std::size_t Size() const { return m_entries.size(); }

    protected:
        struct Entry
        {
            std::uintptr_t key = 0;
            TValue value{};
        };

        void Touch(std::size_t a_index)
        {
            Entry moved = std::move(m_entries[a_index]);
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(a_index));
            m_entries.push_back(std::move(moved));
        }

        std::vector<Entry> m_entries;  // back = most recently used
    };
}
