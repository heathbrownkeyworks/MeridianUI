#pragma once

#include "PCH.h"
#include "Menus/ISubMenu.h"
#include "Menus/CompositorMath.h"

#include <functional>

namespace Meridian::Menus
{
    /// <summary>
    /// Ordered layer registry. Draw order is deterministic: ascending zOrder,
    /// ties broken by creation sequence. Input walks the same snapshot in
    /// reverse (topmost first).
    /// </summary>
    class Compositor
    {
    protected:
        struct Entry
        {
            std::string name;
            std::shared_ptr<ISubMenu> menu;
            std::uint64_t creationSeq = 0;
        };

        mutable std::mutex m_mutex;
        std::vector<Entry> m_entries;
        std::uint64_t m_nextSeq = 1;

        CompositorMath::LayerKey KeyFor(const Entry& a_entry) const;

    public:
        bool Add(std::string_view a_name, std::shared_ptr<ISubMenu> a_menu);
        std::shared_ptr<ISubMenu> Get(const std::string& a_name) const;
        bool Remove(const std::string& a_name, std::shared_ptr<ISubMenu>& a_outRemoved);
        bool Empty() const;
        void Clear(std::vector<std::shared_ptr<ISubMenu>>& a_outMenus);
        std::vector<std::shared_ptr<ISubMenu>> SortedSnapshot() const;
        void ForEach(const std::function<void(ISubMenu&)>& a_visit);
    };
}
