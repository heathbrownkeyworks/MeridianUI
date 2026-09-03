#pragma once

#include "MeridianUIAPI/JSTypes.h"

#include <cstdint>

namespace Meridian::Controllers::PublicAPIValidation
{
    enum class Error
    {
        None,
        NullBrowserName,
        EmptyBrowserName,
        NullStartUrl,
        EmptyStartUrl,
        MissingFunctionArray,
        NullFunctionEntry,
        InvalidFunctionName,
    };

    inline Error ValidateBrowserRequest(const char* a_browserName,
                                        Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                        std::uint32_t a_funcInfoArrSize,
                                        const char* a_startUrl)
    {
        if (a_browserName == nullptr)
        {
            return Error::NullBrowserName;
        }
        if (*a_browserName == '\0')
        {
            return Error::EmptyBrowserName;
        }
        if (a_startUrl == nullptr)
        {
            return Error::NullStartUrl;
        }
        if (*a_startUrl == '\0')
        {
            return Error::EmptyStartUrl;
        }
        if (a_funcInfoArrSize > 0 && a_funcInfoArr == nullptr)
        {
            return Error::MissingFunctionArray;
        }
        for (std::uint32_t i = 0; i < a_funcInfoArrSize; ++i)
        {
            if (a_funcInfoArr[i] == nullptr)
            {
                return Error::NullFunctionEntry;
            }
            if (a_funcInfoArr[i]->objectName == nullptr || *a_funcInfoArr[i]->objectName == '\0' ||
                a_funcInfoArr[i]->funcName == nullptr || *a_funcInfoArr[i]->funcName == '\0')
            {
                return Error::InvalidFunctionName;
            }
        }
        return Error::None;
    }

    inline const char* ToString(Error a_error)
    {
        switch (a_error)
        {
        case Error::None: return "none";
        case Error::NullBrowserName: return "browser name is null";
        case Error::EmptyBrowserName: return "browser name is empty";
        case Error::NullStartUrl: return "start URL is null";
        case Error::EmptyStartUrl: return "start URL is empty";
        case Error::MissingFunctionArray: return "function array is null with a nonzero count";
        case Error::NullFunctionEntry: return "function array contains a null entry";
        case Error::InvalidFunctionName: return "function binding has a null or empty object/function name";
        default: return "unknown validation error";
        }
    }
}
