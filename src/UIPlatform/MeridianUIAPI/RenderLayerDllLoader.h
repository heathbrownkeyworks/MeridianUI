// SPDX-License-Identifier: MIT

#pragma once

#include "RenderLayerAPI.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOGDI
#undef NOMINMAX

namespace Meridian::UI::RenderLayer
{
    inline IRenderLayerAPI* Query(Meridian::UI::Settings* a_settings, const char* a_consumerName)
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
        return static_cast<IRenderLayerAPI*>(result);
    }
}
