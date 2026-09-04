#pragma once

#include "PCH.h"
#include "Common/PressedKeyState.h"

namespace Meridian::Converters
{
    class KeyInputConverter
    {
    protected:
        static inline std::vector<HKL> s_hklVector{};
        static inline size_t s_hklVectorIndex = 0;
        static inline HKL s_currentHKL = (HKL)HKL_NEXT;
        static inline bool s_nativeMenuLangSwitching = false;

        std::uint32_t m_currentModifiers = 0;
        std::uint32_t m_lastScanCode = 0;
        float m_lastKeyHeldDuration = 0.0f;
        RE::ButtonEvent* m_fakeAltTabButtonEvent = nullptr;
        Meridian::Common::PressedKeyState m_pressedKeyState;
        mutable std::recursive_mutex m_stateMutex;

        void KeyDown(const std::uint32_t a_scanCode, const std::uint32_t a_vkCode);
        static bool IsModifierVirtualKey(std::uint32_t a_vkCode);
        void SyncLockModifiers();

    public:
        static void UpdateKeyboardLayouts();
        static void NextKeyboardLayout();
        static HKL GetCurrentKeyboardLayout();
        static void SetNativeMenuLangSwitching(bool a_allow);

        static std::uint32_t GetVirtualKey(const std::uint32_t a_scanCode);
        static wchar_t VkCodeToChar(const std::uint32_t a_scanCode,
                                    const std::uint32_t a_vkCode,
                                    std::uint32_t a_modifiers);

        sigslot::signal_st<CefKeyEvent&> OnKeyDown;
        sigslot::signal_st<CefKeyEvent&> OnKeyUp;
        sigslot::signal_st<CefKeyEvent&> OnChar;

        void Clear();
        void ReleasePressedKeys();
        void UpdateCefKeyModifiers(const cef_event_flags_t a_flags, bool a_isKeyDown);
        void UpdateModifiersFromVK(const std::uint32_t a_vkCode, bool a_isKeyDown);
        std::uint32_t GetCurrentModifiers();
        void ProcessButton(const RE::ButtonEvent* a_event);
        void ProcessAltTab();
    };
}
