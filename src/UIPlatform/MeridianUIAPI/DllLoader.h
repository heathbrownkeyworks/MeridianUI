#pragma once

#include "API.h"

#include <stdexcept>
#include <format>

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOGDI
#undef NOMINMAX

#define NameOf(x) #x

namespace Meridian::UI::DllLoader
{
    inline HMODULE GetMeridianUILib()
    {
        const auto handle = LoadLibrary(Meridian::UI::LibVersion::PROJECT_NAME);
        if (handle == nullptr)
        {
            const auto errCode = GetLastError();
            switch (errCode)
            {
            case 126:
                throw std::runtime_error(std::format("{}.dll not found", Meridian::UI::LibVersion::PROJECT_NAME));
                break;
            default:
                throw std::runtime_error(std::format("Error while loading {}.dll, code: {}", Meridian::UI::LibVersion::PROJECT_NAME, errCode));
                break;
            }
        }

        return handle;
    }

    inline ResponseVersionMessage GetUIPlatformAPIVersion()
    {
        const auto funcPtr = reinterpret_cast<decltype(&GetUIPlatformAPIVersion)>(GetProcAddress(GetMeridianUILib(), NameOf(GetUIPlatformAPIVersion)));
        return funcPtr();
    }

    inline bool CreateOrGetUIPlatformAPI(IUIPlatformAPI** a_outApi, Meridian::UI::Settings* a_settings)
    {
        const auto funcPtr = reinterpret_cast<decltype(&CreateOrGetUIPlatformAPI)>(GetProcAddress(GetMeridianUILib(), NameOf(CreateOrGetUIPlatformAPI)));
        if (funcPtr == nullptr)
        {
            return false;
        }

        return funcPtr(a_outApi, a_settings);
    }

    inline bool CreateOrGetUIPlatformAPIWithVersionCheck(Meridian::UI::IUIPlatformAPI** a_outApi,
                                                         Meridian::UI::Settings* a_settings,
                                                         std::uint32_t a_requestLibVersion,
                                                         const char* a_requestLibName)
    {
        const auto funcPtr = reinterpret_cast<decltype(&CreateOrGetUIPlatformAPIWithVersionCheck)>(GetProcAddress(GetMeridianUILib(), NameOf(CreateOrGetUIPlatformAPIWithVersionCheck)));
        if (funcPtr == nullptr)
        {
            return false;
        }

        return funcPtr(a_outApi, a_settings, a_requestLibVersion, a_requestLibName);
    }
}
