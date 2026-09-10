// SPDX-License-Identifier: MIT
#pragma once
#include "ViewAPI.h"
#include <cstdint>
#include <cstring>

namespace Meridian::UI::Input
{
    inline constexpr char EXTENSION_NAME[] = "Meridian.Input";
    inline constexpr std::uint32_t INTERFACE_VERSION = 1;
    using ViewHandle = View::ViewHandle;
    using ShortcutHandle = std::uint64_t;
    enum class Mode : std::uint32_t
    {
        Navigation = 0,
        Cursor = 1
    };
    enum class GlyphFamily : std::uint32_t
    {
        Generic = 0,
        Xbox = 1,
        PlayStation = 2
    };
    enum class Result : std::uint32_t
    {
        Ok = 0,
        InvalidView = 1,
        InvalidArgument = 2,
        Conflict = 3,
        Unavailable = 4,
        ShuttingDown = 5
    };
    enum class Control : std::uint32_t
    {
        None = 0,
        DpadUp = 1,
        DpadDown = 2,
        DpadLeft = 3,
        DpadRight = 4,
        Start = 5,
        Back = 6,
        LeftThumb = 7,
        RightThumb = 8,
        LeftShoulder = 9,
        RightShoulder = 10,
        South = 11,
        East = 12,
        West = 13,
        North = 14,
        LeftTrigger = 15,
        RightTrigger = 16,
        LeftStick = 17,
        RightStick = 18
    };
    enum class Action : std::uint32_t
    {
        None = 0,
        Up = 1,
        Down = 2,
        Left = 3,
        Right = 4,
        Accept = 5,
        Cancel = 6,
        PreviousTab = 7,
        NextTab = 8,
        Secondary = 9,
        Tertiary = 10,
        ToggleCursor = 11
    };
    inline constexpr std::uint32_t BINDING_COUNT = 11;
    struct ViewInputConfig
    {
        std::uint32_t structSize = sizeof(ViewInputConfig);
        std::uint32_t enabled = 0;
        std::uint32_t allowCursor = 1;
        Mode defaultMode = Mode::Navigation;
        Control bindings[BINDING_COUNT] = {Control::DpadUp, Control::DpadDown, Control::DpadLeft, Control::DpadRight, Control::South, Control::East, Control::LeftShoulder, Control::RightShoulder, Control::West, Control::North, Control::RightThumb};
        std::uint32_t reserved[3] = {};
    };
    struct InputState
    {
        std::uint32_t structSize = sizeof(InputState);
        std::uint32_t enabled = 0;
        std::uint32_t connected = 0;
        std::uint32_t capturing = 0;
        Mode mode = Mode::Navigation;
        GlyphFamily glyphFamily = GlyphFamily::Xbox;
        std::uint64_t generation = 0;
        std::uint32_t reserved[4] = {};
    };
    using ShortcutCallback = void(__cdecl*)(ShortcutHandle, void*);
    struct ShortcutInfo
    {
        std::uint32_t structSize = sizeof(ShortcutInfo);
        Control button = Control::None;
        Control modifier = Control::None;
        std::uint32_t reserved = 0;
        ShortcutCallback callback = nullptr;
        void* userData = nullptr;
    };
    inline bool IsSupported(const char* name, std::uint32_t version)
    {
        return name && std::strcmp(name, EXTENSION_NAME) == 0 && version == INTERFACE_VERSION;
    }
    // Configuration/query calls may originate on any thread. Shortcut callbacks
    // execute on the game thread outside platform locks. Destroy/unregister
    // cancels queued calls; a callback already running must finish before unloading
    // its DLL or freeing userData. Skyrim consumers normally remain loaded.
    class IInputAPI
    {
    public:
        virtual ~IInputAPI() = default;
        virtual Result __cdecl ConfigureView(ViewHandle, const ViewInputConfig*) = 0;
        virtual Result __cdecl GetState(ViewHandle, InputState*) const = 0;
        virtual Result __cdecl RegisterShortcut(ViewHandle, const ShortcutInfo*, ShortcutHandle*) = 0;
        virtual void __cdecl UnregisterShortcut(ShortcutHandle) = 0;
    };
}
