// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace Meridian::UI::LibVersion
{
    inline constexpr std::uint32_t MAJOR = 1;
    inline constexpr std::uint32_t MINOR = 2;
    inline constexpr std::uint32_t PATCH = 0;
    inline constexpr auto PROJECT_NAME = "MeridianUI";

    inline constexpr auto MAJOR_MULT = 100000;
    inline constexpr auto AS_STRING = "1.2.0";
    inline constexpr std::uint32_t AS_INT = (static_cast<std::uint32_t>(MAJOR * MAJOR_MULT + MINOR));
	
    inline std::uint32_t GetMajorVersion(std::uint32_t a_version)
    {
        return a_version / MAJOR_MULT;
    }

    inline std::uint32_t GetMinorVersion(std::uint32_t a_version)
    {
        return a_version - GetMajorVersion(a_version) * MAJOR_MULT;
    }
}

namespace Meridian::UI::APIVersion
{
    inline constexpr std::uint32_t MAJOR = 1;
    inline constexpr std::uint32_t MINOR = 0;

    inline constexpr auto MAJOR_MULT = 100000;
    inline constexpr auto AS_STRING = "1.0";
    inline constexpr std::uint32_t AS_INT = (static_cast<std::uint32_t>(MAJOR * MAJOR_MULT + MINOR));
	
    inline std::uint32_t GetMajorVersion(std::uint32_t a_version)
    {
        return a_version / MAJOR_MULT;
    }

    inline std::uint32_t GetMinorVersion(std::uint32_t a_version)
    {
        return a_version - GetMajorVersion(a_version) * MAJOR_MULT;
    }

    /// <summary>
    /// The single compatibility rule for API requests: same major, minor no
    /// newer than ours. Both the DLL export gate and VersionGateTests call
    /// this — the rule exists exactly once.
    /// </summary>
    inline constexpr bool IsCompatible(std::uint32_t a_requested)
    {
        return a_requested / MAJOR_MULT == MAJOR &&
               a_requested % MAJOR_MULT <= MINOR;
    }
}
