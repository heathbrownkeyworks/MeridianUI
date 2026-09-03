#pragma once

#include "PCH.h"
#include "Render/IRenderLayer.h"
#include "Menus/LayerGeometryHolder.h"

#include <memory>

namespace Meridian::Menus
{
    enum class SubMenuType : std::uint16_t
    {
        CEFMenu = 0,
        NativeSurface,

        Total,
    };

    class ISubMenu : public Meridian::Render::IRenderLayer
    {
    public:
        virtual ~ISubMenu() override = default;
        virtual SubMenuType GetMenuType() = 0;

        // Meridian owns this dispatch contract. It must not inherit the
        // engine MenuEventHandler vtable, whose slots changed in AE 1.7.99.
        virtual bool CanProcess(RE::InputEvent* a_event) = 0;
        virtual bool ProcessMouseMove(RE::MouseMoveEvent* a_event) { return false; }
        virtual bool ProcessButton(RE::ButtonEvent* a_event) { return false; }

        virtual std::shared_ptr<LayerGeometryHolder> GetGeometryHolder() const
        {
            return nullptr;
        }

        virtual void OnResolutionChanged(int a_oldW, int a_oldH, int a_newW, int a_newH) {}

        virtual bool ProcessToggleKeys(RE::ButtonEvent* a_event) { return false; }
    };
}
