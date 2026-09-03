#include "Controllers/RenderLayerAPIController.h"

#include "Menus/NativeSurfaceMenu.h"
#include "Render/RenderHost.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

namespace Meridian::Controllers
{
    Meridian::UI::RenderLayer::SurfaceHandle __cdecl RenderLayerAPIController::CreateSurface(
        const Meridian::UI::RenderLayer::SurfaceCreateInfo* a_info)
    {
        using namespace Meridian::UI::RenderLayer;

        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < SURFACE_CREATE_INFO_MIN_SIZE_1 ||
            a_info->ownerName == nullptr ||
            a_info->surfaceName == nullptr ||
            !IsSafeName(a_info->ownerName) ||
            !IsSafeName(a_info->surfaceName) ||
            !IsValidRect(a_info->x, a_info->y, a_info->width, a_info->height))
        {
            return INVALID_SURFACE_HANDLE;
        }

        const std::string compositorName =
            fmt::format("Meridian.RenderLayer/{}/{}", a_info->ownerName, a_info->surfaceName);
        {
            std::lock_guard lock(m_mutex);
            if (m_isShuttingDown.load(std::memory_order_acquire) ||
                m_ownedCompositorNames.contains(compositorName))
            {
                return INVALID_SURFACE_HANDLE;
            }
            m_ownedCompositorNames.insert(compositorName);
        }

        auto surface = std::make_shared<Meridian::Menus::NativeSurfaceMenu>(
            a_info->x,
            a_info->y,
            a_info->width,
            a_info->height,
            a_info->zOrder,
            a_info->initiallyVisible);

        auto& renderHost = Meridian::Render::RenderHost::GetSingleton();
        bool addedToCompositor = false;
        try
        {
            addedToCompositor = renderHost.AddSubMenu(compositorName, surface);
            if (!addedToCompositor)
            {
                std::lock_guard lock(m_mutex);
                m_ownedCompositorNames.erase(compositorName);
                return INVALID_SURFACE_HANDLE;
            }
            if (!surface->IsReady())
            {
                renderHost.RemoveSubMenu(compositorName);
                std::lock_guard lock(m_mutex);
                m_ownedCompositorNames.erase(compositorName);
                return INVALID_SURFACE_HANDLE;
            }
        }
        catch (const std::exception& error)
        {
            spdlog::error("{}: failed to initialize '{}': {}",
                          NameOf(RenderLayerAPIController), compositorName, error.what());
            if (addedToCompositor)
            {
                renderHost.RemoveSubMenu(compositorName);
            }
            std::lock_guard lock(m_mutex);
            m_ownedCompositorNames.erase(compositorName);
            return INVALID_SURFACE_HANDLE;
        }
        catch (...)
        {
            spdlog::error("{}: failed to initialize '{}'", NameOf(RenderLayerAPIController), compositorName);
            if (addedToCompositor)
            {
                renderHost.RemoveSubMenu(compositorName);
            }
            std::lock_guard lock(m_mutex);
            m_ownedCompositorNames.erase(compositorName);
            return INVALID_SURFACE_HANDLE;
        }

        const auto handle = m_nextHandle.fetch_add(1, std::memory_order_relaxed);
        if (handle == INVALID_SURFACE_HANDLE)
        {
            renderHost.RemoveSubMenu(compositorName);
            std::lock_guard lock(m_mutex);
            m_ownedCompositorNames.erase(compositorName);
            return INVALID_SURFACE_HANDLE;
        }

        auto entry = std::make_shared<SurfaceEntry>();
        entry->handle = handle;
        entry->compositorName = compositorName;
        entry->surface = std::move(surface);

        {
            std::lock_guard lock(m_mutex);
            if (m_isShuttingDown.load(std::memory_order_acquire))
            {
                m_ownedCompositorNames.erase(compositorName);
            }
            else
            {
                m_surfaces.emplace(handle, entry);
                spdlog::info("{}: created {} as handle {}",
                             NameOf(RenderLayerAPIController), compositorName, handle);
                return handle;
            }
        }

        renderHost.RemoveSubMenu(compositorName);
        return INVALID_SURFACE_HANDLE;
    }

    void __cdecl RenderLayerAPIController::DestroySurface(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface)
    {
        std::shared_ptr<SurfaceEntry> entry;
        {
            std::lock_guard lock(m_mutex);
            const auto it = m_surfaces.find(a_surface);
            if (it == m_surfaces.end())
            {
                return;
            }
            entry = std::move(it->second);
            m_surfaces.erase(it);
            m_ownedCompositorNames.erase(entry->compositorName);
        }

        if (!Meridian::Render::RenderHost::GetSingleton().RemoveSubMenu(entry->compositorName) &&
            entry->surface != nullptr)
        {
            entry->surface->BeginShutdown();
        }
    }

    bool __cdecl RenderLayerAPIController::IsValid(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        return GetEntry(a_surface) != nullptr;
    }

    bool __cdecl RenderLayerAPIController::SetRect(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface,
        std::int32_t a_x,
        std::int32_t a_y,
        std::int32_t a_width,
        std::int32_t a_height)
    {
        if (!IsValidRect(a_x, a_y, a_width, a_height))
        {
            return false;
        }
        const auto entry = GetEntry(a_surface);
        if (entry == nullptr || entry->surface == nullptr)
        {
            return false;
        }
        entry->surface->GetGeometryHolder()->SetRect(a_x, a_y, a_width, a_height);
        return true;
    }

    bool __cdecl RenderLayerAPIController::SetZOrder(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface,
        std::int32_t a_zOrder)
    {
        const auto entry = GetEntry(a_surface);
        if (entry == nullptr || entry->surface == nullptr)
        {
            return false;
        }
        entry->surface->GetGeometryHolder()->SetZOrder(a_zOrder);
        return true;
    }

    bool __cdecl RenderLayerAPIController::SetVisible(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface,
        bool a_visible)
    {
        const auto entry = GetEntry(a_surface);
        if (entry == nullptr || entry->surface == nullptr)
        {
            return false;
        }
        entry->surface->SetVisible(a_visible);
        return true;
    }

    bool __cdecl RenderLayerAPIController::IsVisible(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        const auto entry = GetEntry(a_surface);
        return entry != nullptr && entry->surface != nullptr && entry->surface->GetVisible();
    }

    void RenderLayerAPIController::BeginShutdown()
    {
        if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        std::vector<std::shared_ptr<SurfaceEntry>> entries;
        {
            std::lock_guard lock(m_mutex);
            entries.reserve(m_surfaces.size());
            for (auto& [handle, entry] : m_surfaces)
            {
                entries.push_back(std::move(entry));
            }
            m_surfaces.clear();
            m_ownedCompositorNames.clear();
        }

        for (const auto& entry : entries)
        {
            if (entry != nullptr && entry->surface != nullptr)
            {
                entry->surface->BeginShutdown();
            }
        }
    }

    bool RenderLayerAPIController::IsShuttingDown() const
    {
        return m_isShuttingDown.load(std::memory_order_acquire);
    }

    std::shared_ptr<Meridian::Menus::NativeSurfaceMenu>
        RenderLayerAPIController::GetNativeSurface(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        const auto entry = GetEntry(a_surface);
        return entry == nullptr ? nullptr : entry->surface;
    }

    bool RenderLayerAPIController::IsSafeName(std::string_view a_name)
    {
        if (a_name.empty() || a_name.size() > 96)
        {
            return false;
        }
        return std::all_of(a_name.begin(), a_name.end(), [](char c) {
            const auto value = static_cast<unsigned char>(c);
            return std::isalnum(value) != 0 || c == '_' || c == '-' || c == '.' || c == '$';
        });
    }

    bool RenderLayerAPIController::IsValidRect(
        std::int32_t a_x,
        std::int32_t a_y,
        std::int32_t a_width,
        std::int32_t a_height)
    {
        if (a_width <= 0 || a_height <= 0)
        {
            return false;
        }
        const auto right = static_cast<std::int64_t>(a_x) + a_width;
        const auto bottom = static_cast<std::int64_t>(a_y) + a_height;
        return right <= std::numeric_limits<std::int32_t>::max() &&
               right >= std::numeric_limits<std::int32_t>::min() &&
               bottom <= std::numeric_limits<std::int32_t>::max() &&
               bottom >= std::numeric_limits<std::int32_t>::min();
    }

    std::shared_ptr<RenderLayerAPIController::SurfaceEntry> RenderLayerAPIController::GetEntry(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return nullptr;
        }
        std::lock_guard lock(m_mutex);
        const auto it = m_surfaces.find(a_surface);
        return it == m_surfaces.end() ? nullptr : it->second;
    }
}
