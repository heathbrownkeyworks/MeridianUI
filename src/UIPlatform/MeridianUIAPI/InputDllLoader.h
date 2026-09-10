// SPDX-License-Identifier: MIT
#pragma once
#include "InputAPI.h"
#include "ViewDllLoader.h"
namespace Meridian::UI::Input
{
    inline IInputAPI* Query(Settings* settings, const char* consumerName)
    {
        auto module = GetModuleHandleW(L"MeridianUI.dll");
        if (!module)
            return nullptr;
        auto query = reinterpret_cast<View::QueryMeridianExtensionFn>(GetProcAddress(module, "QueryMeridianExtension"));
        void* result = nullptr;
        return query && query(EXTENSION_NAME, INTERFACE_VERSION, &result, settings, consumerName)
                   ? static_cast<IInputAPI*>(result)
                   : nullptr;
    }
}
