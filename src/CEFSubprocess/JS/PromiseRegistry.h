#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Meridian::JS
{
    /// <summary>
    /// Renderer-side bookkeeping for in-flight JS promises. Single-threaded
    /// by contract (the CEF renderer thread owns every call). Entries belong
    /// to a context key so a released context can drain exactly its own
    /// pending promises.
    /// </summary>
    template <typename TPromise, typename TContextKey>
    class PromiseRegistry
    {
    public:
        struct Entry
        {
            TPromise promise{};
            TContextKey contextKey{};
        };

        std::int32_t Register(TPromise a_promise, TContextKey a_contextKey)
        {
            const auto id = m_nextId++;
            m_entries.emplace(id, Entry{std::move(a_promise), std::move(a_contextKey)});
            return id;
        }

        /// <summary>Removes and returns the entry, or false when the id is
        /// unknown (already settled or drained — an expected race).</summary>
        bool Take(std::int32_t a_id, Entry& a_out)
        {
            const auto it = m_entries.find(a_id);
            if (it == m_entries.end())
            {
                return false;
            }
            a_out = std::move(it->second);
            m_entries.erase(it);
            return true;
        }

        /// <summary>Removes every entry for the context and returns them, so
        /// the caller can reject each before the context dies.</summary>
        std::vector<Entry> DrainContext(const TContextKey& a_contextKey)
        {
            std::vector<Entry> drained;
            for (auto it = m_entries.begin(); it != m_entries.end();)
            {
                if (it->second.contextKey == a_contextKey)
                {
                    drained.push_back(std::move(it->second));
                    it = m_entries.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            return drained;
        }

        std::size_t Size() const { return m_entries.size(); }

    protected:
        std::int32_t m_nextId = 1;
        std::unordered_map<std::int32_t, Entry> m_entries;
    };
}
