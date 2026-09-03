#pragma once

#include "PCH.h"
#include "Render/RenderData.h"
#include "Render/PerfProbe.h"
#include "Render/LogThrottle.h"
#include "Menus/Compositor.h"

#include <wrl/client.h>

#include <chrono>
#include <cstdint>

namespace Meridian::Render
{
    /// <summary>
    /// Owns the layer registry and draws it onto the backbuffer at present
    /// time (always-on-top by design). Replaces the always-open
    /// MultiLayerMenu, whose mere presence in the menu stack corrupted
    /// vanilla cursor and console rendering in the original implementation.
    /// </summary>
    class RenderHost
    {
    public:
        static RenderHost& GetSingleton();

        bool Init(std::shared_ptr<spdlog::logger> a_logger);
        void OnPresent();

        RenderData* GetRenderData();
        Meridian::Menus::Compositor& GetCompositor();

        bool AddSubMenu(std::string_view a_menuName, std::shared_ptr<Meridian::Menus::ISubMenu> a_subMenu);
        std::shared_ptr<Meridian::Menus::ISubMenu> GetSubMenu(const std::string& a_menuName);
        bool IsSubMenuExist(const std::string& a_menuName);
        bool RemoveSubMenu(const std::string& a_menuName);
        void BeginShutdown();
        void ClearAllSubMenu();

    protected:
        bool EnsureBackbufferTarget(Microsoft::WRL::ComPtr<ID3D11Texture2D>& a_backbuffer);

        std::shared_ptr<spdlog::logger> m_logger;
        RenderData m_renderData;
        Meridian::Menus::Compositor m_compositor;
        PerfProbe m_perfProbe;

        Microsoft::WRL::ComPtr<ID3D11Device3> m_device3;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext3> m_immediateContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;

        std::atomic_bool m_inited{false};
        std::atomic_bool m_isShuttingDown{false};
        LogThrottle m_getBufferFailThrottle;
    };
}
