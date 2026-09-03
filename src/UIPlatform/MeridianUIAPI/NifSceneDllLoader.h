#pragma once

#include "NifSceneAPI.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOGDI
#undef NOMINMAX

namespace Meridian::UI::NifScene
{
    inline INifSceneAPI* Query(Meridian::UI::Settings* a_settings, const char* a_consumerName)
    {
        const auto module = GetModuleHandleW(L"MeridianUI.dll");
        if (module == nullptr)
        {
            return nullptr;
        }

        const auto query = reinterpret_cast<QueryMeridianExtensionFn>(
            GetProcAddress(module, "QueryMeridianExtension"));
        if (query == nullptr)
        {
            return nullptr;
        }

        void* result = nullptr;
        if (!query(EXTENSION_NAME,
                   INTERFACE_VERSION,
                   &result,
                   a_settings,
                   a_consumerName))
        {
            return nullptr;
        }
        return static_cast<INifSceneAPI*>(result);
    }

    inline INifSceneAPI2* Query2(Meridian::UI::Settings* a_settings,
                                 const char* a_consumerName)
    {
        const auto module = GetModuleHandleW(L"MeridianUI.dll");
        if (module == nullptr)
        {
            return nullptr;
        }

        const auto query = reinterpret_cast<QueryMeridianExtensionFn>(
            GetProcAddress(module, "QueryMeridianExtension"));
        if (query == nullptr)
        {
            return nullptr;
        }

        void* result = nullptr;
        if (!query(EXTENSION_NAME,
                   WEIGHTED_INTERFACE_VERSION,
                   &result,
                   a_settings,
                   a_consumerName))
        {
            return nullptr;
        }
        return static_cast<INifSceneAPI2*>(result);
    }

    inline INifSceneAPI3* Query3(Meridian::UI::Settings* a_settings,
                                 const char* a_consumerName)
    {
        const auto module = GetModuleHandleW(L"MeridianUI.dll");
        if (module == nullptr)
        {
            return nullptr;
        }

        const auto query = reinterpret_cast<QueryMeridianExtensionFn>(
            GetProcAddress(module, "QueryMeridianExtension"));
        if (query == nullptr)
        {
            return nullptr;
        }

        void* result = nullptr;
        if (!query(EXTENSION_NAME,
                   ARMOR_INTERFACE_VERSION,
                   &result,
                   a_settings,
                   a_consumerName))
        {
            return nullptr;
        }
        return static_cast<INifSceneAPI3*>(result);
    }

    inline INifSceneAPI4* Query4(Meridian::UI::Settings* a_settings,
                                 const char* a_consumerName)
    {
        const auto module = GetModuleHandleW(L"MeridianUI.dll");
        if (module == nullptr)
        {
            return nullptr;
        }

        const auto query = reinterpret_cast<QueryMeridianExtensionFn>(
            GetProcAddress(module, "QueryMeridianExtension"));
        if (query == nullptr)
        {
            return nullptr;
        }

        void* result = nullptr;
        if (!query(EXTENSION_NAME,
                   ACTOR_APPEARANCE_INTERFACE_VERSION,
                   &result,
                   a_settings,
                   a_consumerName))
        {
            return nullptr;
        }
        return static_cast<INifSceneAPI4*>(result);
    }
}
