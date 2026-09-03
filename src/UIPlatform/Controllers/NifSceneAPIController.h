#pragma once

#include "Common/Singleton.h"
#include "MeridianUIAPI/NifSceneAPI.h"

#include <atomic>

namespace Meridian::Controllers
{
    class NifSceneAPIController final : public Meridian::UI::NifScene::INifSceneAPI4,
                                        public Meridian::Common::Singleton<NifSceneAPIController>
    {
        friend class Meridian::Common::Singleton<NifSceneAPIController>;

    public:
        bool __cdecl ReplaceScene(
            const Meridian::UI::NifScene::SceneLoadInfo* a_info) override;
        bool __cdecl ReplaceWeightedScene(
            const Meridian::UI::NifScene::WeightedSceneLoadInfo* a_info) override;
        bool __cdecl ReplaceArmorScene(
            const Meridian::UI::NifScene::ArmorSceneLoadInfo* a_info) override;
        bool __cdecl ReplaceActorAppearanceScene(
            const Meridian::UI::NifScene::ActorAppearanceSceneLoadInfo* a_info) override;
        void __cdecl ClearScene(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) override;
        Meridian::UI::NifView::Status __cdecl GetStatus(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) const override;
        bool __cdecl SetObjectVisible(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface,
            Meridian::UI::NifScene::ObjectHandle a_object,
            bool a_visible) override;
        bool __cdecl FrameScene(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) override;

        void BeginShutdown();
        bool IsShuttingDown() const;

    private:
        NifSceneAPIController() = default;

        std::atomic_bool m_isShuttingDown{false};
    };
}
