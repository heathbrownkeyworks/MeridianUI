#pragma once

#include "CallSiteValidation.h"

#include <REL/Module.h>

#include <cstdint>
#include <span>

namespace Meridian::Hooks
{
    inline bool IsExpectedCallSite(
        std::uintptr_t a_address,
        CallEncoding a_encoding)
    {
        const auto text = REL::Module::get().segment(REL::Segment::Name::textx);
        const auto textAddress = text.address();
        const auto requiredBytes = RequiredCallBytes(a_encoding);

        if (a_address < textAddress)
        {
            return false;
        }

        const auto offset = a_address - textAddress;
        if (offset > text.size() || requiredBytes > text.size() - offset)
        {
            return false;
        }

        const auto bytes = std::span{
            reinterpret_cast<const std::uint8_t*>(a_address),
            requiredBytes};
        return MatchesExpectedCall(bytes, a_encoding);
    }
}
