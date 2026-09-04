#include "KeyInputConverter.h"

namespace Meridian::Converters
{
    void KeyInputConverter::KeyDown(const std::uint32_t a_scanCode, const std::uint32_t a_vkCode)
    {
        CefKeyEvent keyEvent{};
        keyEvent.windows_key_code = a_vkCode;
        keyEvent.native_key_code = a_scanCode;
        keyEvent.modifiers = m_currentModifiers;
        keyEvent.type = KEYEVENT_RAWKEYDOWN;
        OnKeyDown(keyEvent);

        const auto wchar = VkCodeToChar(a_scanCode, a_vkCode, m_currentModifiers);
        if (wchar != 0)
        {
            keyEvent.type = KEYEVENT_CHAR;
            keyEvent.windows_key_code = wchar;
            OnChar(keyEvent);
        }
    }

    std::uint32_t KeyInputConverter::GetVirtualKey(const std::uint32_t a_scanCode)
    {
        if (a_scanCode == RE::BSKeyboardDevice::Keys::kKP_Enter)
        {
            return VK_RETURN;
        }

        std::uint32_t vkCode = 0;
        const auto keyboard = RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        if (keyboard != nullptr)
        {
            keyboard->GetKeyCodeFromID(a_scanCode, vkCode);
        }
        return vkCode;
    }

    wchar_t KeyInputConverter::VkCodeToChar(const std::uint32_t a_scanCode,
                                            const std::uint32_t a_vkCode,
                                            std::uint32_t a_modifiers)
    {
        // auto keyboard = RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        static uint8_t s_state[256] = {0};
        s_state[VK_SHIFT] = (a_modifiers & EVENTFLAG_SHIFT_DOWN) != 0 ? 0x80 : 0;
        s_state[VK_CAPITAL] = (a_modifiers & EVENTFLAG_CAPS_LOCK_ON) != 0 ? 0x01 : 0;
        s_state[VK_NUMLOCK] = (a_modifiers & EVENTFLAG_NUM_LOCK_ON) != 0 ? 0x01 : 0;

        wchar_t unicodeChar;
        if (ToUnicodeEx(a_vkCode, a_scanCode, s_state, &unicodeChar, 1, 0, s_currentHKL) != 1)
        {
            return 0;
        }

        return unicodeChar;
    }

    void KeyInputConverter::UpdateKeyboardLayouts()
    {
        const auto clearHKLs = []() {
            s_hklVector.clear();
            s_hklVectorIndex = 0;
            s_currentHKL = (HKL)HKL_NEXT;
        };

        const auto hklCount = GetKeyboardLayoutList(0, nullptr);
        if (hklCount <= 0)
        {
            spdlog::error("GetKeyboardLayoutList failed, {}", GetLastErrorAsString().data());
            clearHKLs();
            return;
        }

        s_hklVector.resize(hklCount);
        const auto hklCount2 = GetKeyboardLayoutList(hklCount, s_hklVector.data());
        if (hklCount2 <= 0)
        {
            spdlog::error("Second call of GetKeyboardLayoutList failed, {}", GetLastErrorAsString().data());
            clearHKLs();
            return;
        }
        else if (hklCount2 != hklCount)
        {
            spdlog::error("GetKeyboardLayoutList returned a different value than the previous call");
            clearHKLs();
            return;
        }

        spdlog::info("KeyInputConverter: Found {} keyboard layouts", s_hklVector.size());

        s_hklVectorIndex = 0;
        s_currentHKL = s_hklVector[s_hklVectorIndex];
    }

    void KeyInputConverter::NextKeyboardLayout()
    {
        if (s_hklVector.empty())
        {
            s_currentHKL = (HKL)HKL_NEXT;
            return;
        }

        ++s_hklVectorIndex;
        if (s_hklVectorIndex >= s_hklVector.size())
        {
            s_hklVectorIndex = 0;
        }
        s_currentHKL = s_hklVector[s_hklVectorIndex];

        if (s_nativeMenuLangSwitching)
        {
            SKSE::GetTaskInterface()->AddUITask([]() {
                ActivateKeyboardLayout(s_currentHKL, 0);
            });
        }
    }

    HKL KeyInputConverter::GetCurrentKeyboardLayout()
    {
        return s_currentHKL;
    }

    void KeyInputConverter::SetNativeMenuLangSwitching(bool a_allow)
    {
        s_nativeMenuLangSwitching = a_allow;
    }

    void KeyInputConverter::Clear()
    {
        std::lock_guard lock(m_stateMutex);
        m_pressedKeyState.Clear();
        m_currentModifiers = 0;
        m_lastScanCode = 0;
        m_lastKeyHeldDuration = 0.0f;
    }

    bool KeyInputConverter::IsModifierVirtualKey(std::uint32_t a_vkCode)
    {
        switch (a_vkCode)
        {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_CAPITAL:
        case VK_NUMLOCK:
            return true;
        default:
            return false;
        }
    }

    void KeyInputConverter::SyncLockModifiers()
    {
        UpdateCefKeyModifiers(
            EVENTFLAG_CAPS_LOCK_ON,
            (GetKeyState(VK_CAPITAL) & 0x0001) != 0);
        UpdateCefKeyModifiers(
            EVENTFLAG_NUM_LOCK_ON,
            (GetKeyState(VK_NUMLOCK) & 0x0001) != 0);
    }

    void KeyInputConverter::ReleasePressedKeys()
    {
        std::lock_guard lock(m_stateMutex);
        for (const auto& key : m_pressedKeyState.Drain())
        {
            UpdateModifiersFromVK(key.virtualKey, false);

            CefKeyEvent keyEvent{};
            keyEvent.windows_key_code = key.virtualKey;
            keyEvent.native_key_code = key.scanCode;
            keyEvent.modifiers = m_currentModifiers;
            keyEvent.type = KEYEVENT_KEYUP;
            OnKeyUp(keyEvent);
        }

        m_currentModifiers = 0;
        m_lastScanCode = 0;
        m_lastKeyHeldDuration = 0.0f;
    }

    void KeyInputConverter::UpdateCefKeyModifiers(const cef_event_flags_t a_flags, bool a_isKeyDown)
    {
        std::lock_guard lock(m_stateMutex);
        if (a_isKeyDown)
        {
            m_currentModifiers |= a_flags;
        }
        else
        {
            m_currentModifiers &= ~a_flags;
        }
    }

    void KeyInputConverter::UpdateModifiersFromVK(const std::uint32_t a_vkCode, bool a_isKeyDown)
    {
        std::lock_guard lock(m_stateMutex);
        if (a_vkCode >= VK_NUMPAD0 && a_vkCode <= VK_DIVIDE)
        {
            UpdateCefKeyModifiers(EVENTFLAG_IS_KEY_PAD, a_isKeyDown);
        }
        else
        {
            switch (a_vkCode)
            {
            case VK_CAPITAL:
                // Lock flags represent the OS toggle state, not the physical
                // key state. SyncLockModifiers refreshes them for every event.
                break;
            case VK_SHIFT:
                UpdateCefKeyModifiers(EVENTFLAG_SHIFT_DOWN, a_isKeyDown);
                break;
            case VK_CONTROL:
                UpdateCefKeyModifiers(EVENTFLAG_CONTROL_DOWN, a_isKeyDown);
                break;
            case VK_MENU:
                UpdateCefKeyModifiers(EVENTFLAG_ALT_DOWN, a_isKeyDown);
                break;
            case VK_NUMLOCK:
                break;
            case VK_LCONTROL:
                UpdateCefKeyModifiers(EVENTFLAG_CONTROL_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_LEFT, a_isKeyDown);
                break;
            case VK_LMENU:
                UpdateCefKeyModifiers(EVENTFLAG_ALT_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_LEFT, a_isKeyDown);
                break;
            case VK_LSHIFT:
                UpdateCefKeyModifiers(EVENTFLAG_SHIFT_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_LEFT, a_isKeyDown);
                break;
            case VK_RCONTROL:
                UpdateCefKeyModifiers(EVENTFLAG_CONTROL_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_RIGHT, a_isKeyDown);
                break;
            case VK_RMENU:
                UpdateCefKeyModifiers(EVENTFLAG_ALT_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_RIGHT, a_isKeyDown);
                break;
            case VK_RSHIFT:
                UpdateCefKeyModifiers(EVENTFLAG_SHIFT_DOWN, a_isKeyDown);
                UpdateCefKeyModifiers(EVENTFLAG_IS_RIGHT, a_isKeyDown);
                break;
            default:
                break;
            }
        }
    }

    std::uint32_t KeyInputConverter::GetCurrentModifiers()
    {
        std::lock_guard lock(m_stateMutex);
        return m_currentModifiers;
    }

    void KeyInputConverter::ProcessButton(const RE::ButtonEvent* a_event)
    {
        std::lock_guard lock(m_stateMutex);
        const auto isKeyStateChanged = a_event->IsDown() || a_event->IsUp();
        if (isKeyStateChanged)
        {
            const auto scanCode = a_event->GetIDCode();
            const auto vkCode = GetVirtualKey(scanCode);
            SyncLockModifiers();
            UpdateModifiersFromVK(vkCode, a_event->IsDown());

            if (a_event->IsDown())
            {
                m_pressedKeyState.Press(scanCode, vkCode, IsModifierVirtualKey(vkCode));
                m_lastScanCode = scanCode;
                m_lastKeyHeldDuration = KEY_FIRST_CHAR_DELAY;

                KeyDown(scanCode, vkCode);
            }
            else
            {
                CefKeyEvent keyEvent{};
                keyEvent.windows_key_code = vkCode;
                keyEvent.native_key_code = scanCode;
                keyEvent.modifiers = m_currentModifiers;
                keyEvent.type = KEYEVENT_KEYUP;

                m_pressedKeyState.Release(scanCode);
                OnKeyUp(keyEvent);
            }
        }
        else if (a_event->GetIDCode() == m_lastScanCode && (a_event->HeldDuration() - m_lastKeyHeldDuration) > KEY_CHAR_REPEAT_DELAY)
        {
            m_lastKeyHeldDuration = a_event->HeldDuration();

            KeyDown(a_event->GetIDCode(), GetVirtualKey(m_lastScanCode));
        }
    }

    void KeyInputConverter::ProcessAltTab()
    {
        std::lock_guard lock(m_stateMutex);
        ReleasePressedKeys();

        if (!m_fakeAltTabButtonEvent)
        {
            m_fakeAltTabButtonEvent = RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard,
                                                              "",
                                                              0,
                                                              0.0f,
                                                              1.0f);
        }

        m_fakeAltTabButtonEvent->idCode = RE::BSKeyboardDevice::Keys::kLeftAlt;
        ProcessButton(m_fakeAltTabButtonEvent);

        m_fakeAltTabButtonEvent->idCode = RE::BSKeyboardDevice::Keys::kTab;
        ProcessButton(m_fakeAltTabButtonEvent);
        ReleasePressedKeys();
    }
}
