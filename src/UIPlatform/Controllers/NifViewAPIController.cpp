#include "Controllers/NifViewAPIController.h"

#include "Controllers/NifLoadGeneration.h"
#include "Controllers/NifModelPath.h"
#include "Controllers/RenderLayerAPIController.h"
#include "Menus/NativeSurfaceMenu.h"

#include <cmath>
#include <memory>
#include <string>

namespace Meridian::Controllers
{
    bool __cdecl NifViewAPIController::LoadModel(
        const Meridian::UI::NifView::NifLoadInfo* a_info)
    {
        using namespace Meridian::UI::NifView;

        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < NIF_LOAD_INFO_MIN_SIZE_1 ||
            a_info->modelPath == nullptr)
        {
            return false;
        }

        std::string modelPath;
        if (!NormalizeNifModelPath(a_info->modelPath, modelPath))
        {
            spdlog::warn("{}: rejected unsafe or invalid NIF path", NameOf(NifViewAPIController));
            return false;
        }

        auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_info->surface);
        if (surface == nullptr || !surface->IsReady())
        {
            return false;
        }

        const auto generation = NextNifLoadGeneration();
        surface->BeginNifLoad(generation);
        const std::weak_ptr<Meridian::Menus::NativeSurfaceMenu> weakSurface = surface;
        const bool frameOnLoad = a_info->frameOnLoad;

        const auto taskInterface = SKSE::GetTaskInterface();
        if (taskInterface == nullptr)
        {
            surface->FailNifLoad(generation, Status::Failed);
            return false;
        }

        spdlog::info("{}: queued '{}' for surface {}",
                     NameOf(NifViewAPIController), modelPath, a_info->surface);

        taskInterface->AddTask([weakSurface,
                                generation,
                                frameOnLoad,
                                modelPath = std::move(modelPath)]() mutable {
            auto& controller = NifViewAPIController::GetSingleton();
            auto target = weakSurface.lock();
            if (controller.IsShuttingDown() || target == nullptr)
            {
                return;
            }

            try
            {
                RE::NiPointer<RE::NiNode> scene;
                const RE::BSModelDB::DBTraits::ArgsType arguments{};
                const auto error = RE::BSModelDB::Demand(modelPath.c_str(), scene, arguments);
                if (error != RE::BSResource::ErrorCode::kNone || scene == nullptr)
                {
                    spdlog::warn("{}: BSModelDB could not load '{}' (error {})",
                                 NameOf(NifViewAPIController),
                                 modelPath,
                                 static_cast<std::int32_t>(error));
                    target->FailNifLoad(generation, Status::Failed);
                    return;
                }

                if (!controller.IsShuttingDown())
                {
                    target->SubmitNifScene(generation, std::move(scene), frameOnLoad);
                }
            }
            catch (const std::exception& error)
            {
                spdlog::error("{}: exception loading '{}': {}",
                              NameOf(NifViewAPIController), modelPath, error.what());
                target->FailNifLoad(generation, Status::Failed);
            }
            catch (...)
            {
                spdlog::error("{}: unknown exception loading '{}'",
                              NameOf(NifViewAPIController), modelPath);
                target->FailNifLoad(generation, Status::Failed);
            }
        });

        return true;
    }

    void __cdecl NifViewAPIController::ClearModel(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        if (surface != nullptr)
        {
            surface->ClearNif(NextNifLoadGeneration());
        }
    }

    Meridian::UI::NifView::Status __cdecl NifViewAPIController::GetStatus(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return Meridian::UI::NifView::Status::ShuttingDown;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface == nullptr ?
            Meridian::UI::NifView::Status::InvalidSurface :
            surface->GetNifStatus();
    }

    bool __cdecl NifViewAPIController::SetCamera(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface,
        const Meridian::UI::NifView::CameraState* a_camera)
    {
        using namespace Meridian::UI::NifView;
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_camera == nullptr ||
            a_camera->structSize < CAMERA_STATE_MIN_SIZE_1 ||
            !std::isfinite(a_camera->yawDegrees) ||
            !std::isfinite(a_camera->pitchDegrees) ||
            !std::isfinite(a_camera->distanceScale) ||
            !std::isfinite(a_camera->panX) ||
            !std::isfinite(a_camera->panY) ||
            !std::isfinite(a_camera->panZ) ||
            a_camera->pitchDegrees < -89.0f ||
            a_camera->pitchDegrees > 89.0f ||
            a_camera->distanceScale < 0.05f ||
            a_camera->distanceScale > 20.0f ||
            std::abs(a_camera->panX) > 10.0f ||
            std::abs(a_camera->panY) > 10.0f ||
            std::abs(a_camera->panZ) > 10.0f)
        {
            return false;
        }

        CameraState normalized{};
        normalized.yawDegrees = a_camera->yawDegrees;
        normalized.pitchDegrees = a_camera->pitchDegrees;
        normalized.distanceScale = a_camera->distanceScale;
        normalized.panX = a_camera->panX;
        normalized.panY = a_camera->panY;
        normalized.panZ = a_camera->panZ;
        if (a_camera->structSize >= CAMERA_STATE_LIGHTING_SIZE_1)
        {
            if (!IsValidLightingPreset(a_camera->lightingPreset) ||
                !std::isfinite(a_camera->exposureStops) ||
                a_camera->exposureStops < -2.0f ||
                a_camera->exposureStops > 2.0f)
            {
                return false;
            }
            normalized.lightingPreset = a_camera->lightingPreset;
            normalized.exposureStops = a_camera->exposureStops;
        }

        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface != nullptr && surface->SetNifCamera(normalized);
    }

    bool __cdecl NifViewAPIController::FrameModel(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return false;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface != nullptr && surface->FrameNif();
    }

    void NifViewAPIController::BeginShutdown()
    {
        m_isShuttingDown.store(true, std::memory_order_release);
    }

    bool NifViewAPIController::IsShuttingDown() const
    {
        return m_isShuttingDown.load(std::memory_order_acquire);
    }

}
