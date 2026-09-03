#pragma once

#include "../PCH.h"
#include "CallSiteGuard.h"
#include "Converters/KeyInputConverter.h"

namespace Meridian::Hooks
{
    class CharGeneratorHook
    {
    protected:
        static int ToUnicodeCall(UINT wVirtKey, UINT wScanCode, const BYTE* lpKeyState, LPWSTR pwszBuff, int cchBuff, UINT wFlags)
        {
            return ToUnicodeEx(wVirtKey, wScanCode, lpKeyState, pwszBuff, cchBuff, wFlags, Meridian::Converters::KeyInputConverter::GetCurrentKeyboardLayout());
        }
        static inline REL::Relocation<decltype(&ToUnicodeCall)> _ToUnicodeCall;

    public:
        static bool Install()
        {
            try
            {
                // ToUnicode call in input processing (SE: 67472+0x20D, AE: 68782+0x2CB)
                REL::Relocation<std::uintptr_t> toUnicodeFunc{RELOCATION_ID(67472, 68782), REL::VariantOffset(0x20D, 0x2CB, 0)};
                if (!IsExpectedCallSite(toUnicodeFunc.address(), CallEncoding::RipRelativeIndirect6))
                {
                    spdlog::error(
                        "{}: install refused at {:X}: expected FF/15 RIP-relative call for Skyrim {}",
                        NameOf(CharGeneratorHook),
                        toUnicodeFunc.address(),
                        REL::Module::get().version().string());
                    return false;
                }
                _ToUnicodeCall = SKSE::GetTrampoline().write_call<6>(toUnicodeFunc.address(), &ToUnicodeCall);
                return true;
            }
            catch (const std::exception& e)
            {
                spdlog::error("{}: install FAILED ({})", NameOf(CharGeneratorHook), e.what());
                return false;
            }
        }
    };
}
