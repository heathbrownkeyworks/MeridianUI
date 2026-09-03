#include "Compositor.h"

namespace Meridian::Menus
{
    CompositorMath::LayerKey Compositor::KeyFor(const Entry& a_entry) const
    {
        const auto holder = a_entry.menu ? a_entry.menu->GetGeometryHolder() : nullptr;
        return CompositorMath::LayerKey{holder ? holder->GetZOrder() : 0, a_entry.creationSeq};
    }

    bool Compositor::Add(std::string_view a_name, std::shared_ptr<ISubMenu> a_menu)
    {
        std::lock_guard lock(m_mutex);
        const std::string name{a_name};
        for (const auto& entry : m_entries)
        {
            if (entry.name == name)
            {
                return false;
            }
        }
        m_entries.push_back(Entry{name, std::move(a_menu), m_nextSeq++});
        return true;
    }

    std::shared_ptr<ISubMenu> Compositor::Get(const std::string& a_name) const
    {
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_entries)
        {
            if (entry.name == a_name)
            {
                return entry.menu;
            }
        }
        return nullptr;
    }

    bool Compositor::Remove(const std::string& a_name, std::shared_ptr<ISubMenu>& a_outRemoved)
    {
        std::lock_guard lock(m_mutex);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
        {
            if (it->name == a_name)
            {
                a_outRemoved = std::move(it->menu);
                m_entries.erase(it);
                return true;
            }
        }
        return false;
    }

    bool Compositor::Empty() const
    {
        std::lock_guard lock(m_mutex);
        return m_entries.empty();
    }

    void Compositor::Clear(std::vector<std::shared_ptr<ISubMenu>>& a_outMenus)
    {
        std::lock_guard lock(m_mutex);
        a_outMenus.reserve(m_entries.size());
        for (auto& entry : m_entries)
        {
            a_outMenus.push_back(std::move(entry.menu));
        }
        m_entries.clear();
    }

    std::vector<std::shared_ptr<ISubMenu>> Compositor::SortedSnapshot() const
    {
        std::vector<std::pair<CompositorMath::LayerKey, std::shared_ptr<ISubMenu>>> keyed;
        {
            std::lock_guard lock(m_mutex);
            keyed.reserve(m_entries.size());
            for (const auto& entry : m_entries)
            {
                keyed.emplace_back(KeyFor(entry), entry.menu);
            }
        }

        std::stable_sort(keyed.begin(), keyed.end(),
                         [](const auto& a, const auto& b) { return CompositorMath::DrawsBefore(a.first, b.first); });

        std::vector<std::shared_ptr<ISubMenu>> result;
        result.reserve(keyed.size());
        for (auto& [key, menu] : keyed)
        {
            result.push_back(std::move(menu));
        }
        return result;
    }

    void Compositor::ForEach(const std::function<void(ISubMenu&)>& a_visit)
    {
        std::vector<std::shared_ptr<ISubMenu>> menus;
        {
            std::lock_guard lock(m_mutex);
            menus.reserve(m_entries.size());
            for (const auto& entry : m_entries)
            {
                menus.push_back(entry.menu);
            }
        }
        for (const auto& menu : menus)
        {
            if (menu != nullptr)
            {
                a_visit(*menu);
            }
        }
    }
}
