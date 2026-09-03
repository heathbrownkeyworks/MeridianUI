#pragma once

#include <SKSE/SKSE.h>

#include <cstdint>
#include <string_view>

namespace Meridian::RuntimeCompatibility
{
    constexpr SKSE::PluginVersionData MakePluginVersionData(
        std::uint32_t a_pluginVersion,
        std::string_view a_pluginName)
    {
        SKSE::PluginVersionData version{};
        version.pluginVersion = a_pluginVersion;
        version.PluginName(a_pluginName);
        version.AuthorName("ColdSun");
        version.UsesAddressLibrary();
        version.UsesUpdatedStructs();
        return version;
    }

    constexpr bool HasAddressLibraryV5(const SKSE::PluginVersionData& a_version)
    {
        return (a_version.versionIndependenceEx &
                SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5) != 0;
    }

    constexpr bool HasUpdatedStructs(const SKSE::PluginVersionData& a_version)
    {
        return (a_version.versionIndependence &
                SKSE::PluginVersionData::kVersionIndependent_StructsPost629) != 0;
    }
}
