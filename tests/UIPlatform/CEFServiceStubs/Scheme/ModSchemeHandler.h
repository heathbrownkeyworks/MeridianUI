#pragma once
#include "PCH.h"

namespace Meridian::Scheme { struct ModSchemeHandlerFactory {}; }

inline bool CefRegisterSchemeHandlerFactory(const char*, const char*, Meridian::Scheme::ModSchemeHandlerFactory* a_factory)
{
    ++CefCalls::registerScheme;
    delete a_factory;
    return true;
}
