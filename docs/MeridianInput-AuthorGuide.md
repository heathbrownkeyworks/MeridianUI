# Meridian.Input/1 consumer guide

Status: development candidate on the controllersupport branch. Feature-detect Input/1; a version string alone does not establish availability. See [controller validation](testing/MERIDIAN_CONTROLLER_RUNTIME_GATE.md). In-game controller validation is separate from native/browser tests.

## Integration

Keep the existing View/1 interface and normal UI lifecycle. Query the optional Input/1 extension from the loaded Meridian runtime, configure each view, and use the injected page helper. Missing Input/1 leaves existing keyboard/mouse operation available. Consumers ship their own DLL/web assets and the public headers they compile against.

```cpp
#include "MeridianUIAPI/ViewDllLoader.h"
#include "MeridianUIAPI/InputDllLoader.h"

Meridian::UI::View::IViewAPI* viewAPI = nullptr;
Meridian::UI::Input::IInputAPI* inputAPI = nullptr;

void QueryInterfaces()
{
    Meridian::UI::Settings settings{};
    viewAPI = Meridian::UI::View::Query(&settings, "MyMod");
    inputAPI = Meridian::UI::Input::Query(&settings, "MyMod");
}

bool EnableController(Meridian::UI::View::ViewHandle view)
{
    if (!inputAPI) return false;
    Meridian::UI::Input::ViewInputConfig config{};
    config.enabled = 1;
    config.allowCursor = 1;
    return inputAPI->ConfigureView(view, &config) ==
        Meridian::UI::Input::Result::Ok;
}
```

Call EnableController after creating a valid view, typically from its DOM-ready callback. Native configuration persists through page reloads. The helper is injected before the consumer DOM-ready notification; ordinary named listeners may finish registering afterward, so wait for the consumer callback you need before using it.

The repository's `src/InputTest/main.cpp` is a complete compiling diagnostic consumer, including startup, global shortcuts, two views and close/reload/destruction. Build it with MERIDIAN_BUILD_INPUT_TEST; its fixture assets stage separately from the runtime.

## Native ABI

The exact contract is `MeridianUIAPI/InputAPI.h`, included in the staged SDK. Query name is case-sensitive Meridian.Input; interface version is 1. InputDllLoader uses GetModuleHandleW and the existing QueryMeridianExtension export. It does not load another runtime DLL.

| Method | Contract |
|---|---|
| ConfigureView(view, config) | Validates a live View/1 handle and copies config. Default enabled is 0. Native configuration controls whether the page may use cursor mode. |
| GetState(view, output) | Caller initializes structSize. Copies enablement, connection, capture, mode, prompt family and generation. This is state, not a raw polling API. |
| RegisterShortcut(view, info, outputHandle) | Optional digital opener bound to a view's lifetime. Copies descriptor. Returns Conflict for duplicate/ambiguous chords. Output is zero on failure. |
| UnregisterShortcut(handle) | Removes registration and cancels a queued invocation that has not begun delivery. DestroyView also removes that view's shortcuts. |

Records use fixed-width integers, explicit enum values, structSize and reserved zero fields. No STL object crosses the DLL boundary. ViewInputConfig has eleven bindings, indexed by Action minus one. Control::None unbinds an action. Other bindings must use a unique digital control. Triggers/sticks remain available as analog controls and cannot be global shortcuts in v1.

Configuration/query methods are synchronized and may be called from any consumer thread. **Shortcut callbacks run on the game thread, outside platform locks.** Unregister/destroy cancels queued callbacks; a callback already admitted for execution may finish. Do not unload its code or free userData until it finishes. Ordinary SKSE consumers remain loaded.

View/1 JS listener callbacks still run on Meridian's CEF callback thread. Copy payloads and marshal Skyrim changes through SKSE's task interface. Controller support does not change that existing rule.

## Opening and closing

A ShortcutInfo contains a digital button, optional distinct digital modifier, callback and caller data. The callback should invoke the mod's existing opening code. No global opener is assigned automatically. The fixture registers LeftShoulder + Start.

Registration rejects equal activation buttons, single buttons overlapping a chord, and reversed equivalent chords. Two chords with a common modifier and different activation buttons can coexist. Only the completing button-down edge activates a chord; holding it does not repeat. One pending opener is admitted at a time.

Shortcuts are gated by foreground/gameplay state, pause/loading/console/dialogue/character-creation menus and the absence of another Meridian focus owner. Unmatched chord prefixes retain gameplay behavior. The activation is consumed and its release is balanced. The opening press cannot activate the first selected UI item.

While focused, cancel/back belongs to the consumer. Connect it to the existing close/unsaved-change/preview-cleanup path. Meridian does not directly hide a consumer when B is pressed. Clear your page scopes when replacing screens, and disable a closing screen locally before asynchronous native cleanup if it can receive another action.

## Page helper

window.MeridianInput is installed in View/1 documents. It has version 1.

| API | Purpose |
|---|---|
| getState() | Immutable current state and bindings. |
| onAction(handler) | Subscribe to normalized actions/controls. Returns unsubscribe. Return true to handle an action before shared defaults. |
| onStateChange(handler) | Subscribe to immutable state changes. Returns unsubscribe. |
| setMode('navigation' or 'cursor') | Request mode. Native policy decides; observe getState/onStateChange for confirmation. |
| getPrompt(action) | Returns action, control, family and label derived from current bindings. |
| attachNavigation(options) | Push a scope. Returns an idempotent dispose function. |

__receive is reserved for Meridian's transport and is not a consumer input API. Do not poll navigator.getGamepads, install XInput polling, call OS SendInput, or add a competing game-input capture hook.

A minimal scope:

```javascript
const input = window.MeridianInput;
let dispose;
if (input?.version === 1) {
    dispose = input.attachNavigation({
        root: document.querySelector('#settings'),
        initialFocus: 'first-setting',
        onBack() {
            dispose();
            window.myModClose('');
        }
    });
    const stopActions = input.onAction(event => {
        if (event.action === 'secondary' && event.phase === 'press') {
            window.myModSecondary('');
            return true;
        }
        return false;
    });
    // Call stopActions() when this screen's handler is no longer needed.
}
```

Payloads have control, action, phase, value, x, y, generation, sequence and dt. dt uses real seconds and is capped at 0.05 after stalls. Control names are positional: south/east/west/north, dpadUp/Down/Left/Right, left/rightShoulder, left/rightThumb, start/back, left/rightTrigger and left/rightStick. Names do not imply physical branding.

Phases: press, release, repeat, change, cancel. Navigation begins immediately and repeats after the configured delay. Accept/cancel/secondary/toggle actions never repeat automatically. A lifecycle cancel is broadcast to subscribers so custom held-state handlers can reset. Use state.active as well; page replacement destroys subscriptions and requires normal DOM-ready setup again.

Input carries a page identity, focus generation and ordered sequence. Stale, wrong-page, malformed and oversized packets are discarded. Native queued delivery is bounded and canceled when its browser epoch changes. Actions are not replayed across reload/focus transitions.

## Navigation scopes

Each scope has root and optional initialFocus, onBack, onAction, wrap and getCandidates. initialFocus may be a stable ID, an element or a function returning an element. getCandidates returns rendered elements for a custom/virtualized widget.

Enabled native controls and explicitly focusable elements participate. Disabled, hidden, inert, aria-disabled and negative-tabindex elements are excluded. Add tabindex=0 to custom interactive elements. For stable restoration use id or data-meridian-id. An explicit data-meridian-up/down/left/right value targets another element's stable ID. Otherwise the helper selects a directional geometric neighbor with stable tie ordering. Wrapping is opt-in.

Opening a modal calls attachNavigation again with the modal root. The newest scope handles navigation. Dispose it to restore the previous selection. Remove handlers/scopes when a screen is destroyed. Filtering/replacing a selected element restores its stable ID when possible or selects a valid fallback.

The helper marks the selected element with data-meridian-focused. The mod owns its CSS:

```css
[data-meridian-focused] {
    outline: 3px solid var(--focus-color, #f9c568);
    outline-offset: 3px;
}
```

HTML role=tab controls support previousTab/nextTab using the existing click handler. Standard range/select controls enter edit mode on accept, change value with direction, and leave edit mode on accept/cancel. Custom controls can consume actions in onAction. Right-stick input scrolls the nearest scrollable ancestor of the selection, or the pointer target in cursor mode.

## Default bindings and prompts

| Control | Action |
|---|---|
| D-pad / left stick | Focus navigation |
| South / A | Accept |
| East / B | Cancel/back |
| Left/right shoulder | Previous/next tab |
| West / X, North / Y | Secondary/tertiary |
| Right-stick click | Toggle optional cursor |
| Right stick | Scroll; custom handler may consume |
| Triggers, Start, Back, left-stick click | Exposed; no UI action assigned by default |

D-pad/navigation bindings return from cursor to focus navigation. In cursor mode, the left stick moves the shared cursor and accept sends one balanced CEF mouse sequence. It does not also activate the selected DOM element. Mouse input switches prompt/pointer presentation; idle drift and synthesized pointer movement do not count as a physical device switch. Cursor position is shared with normal mouse routing.

The document element receives data-meridian-input and data-meridian-mode for styling. Prompt lookup supports Xbox, PlayStation and generic labels. A translation layer may hide physical device identity; the runtime INI has an explicit family preference. These are labels; style your own footer/action descriptions.

## Analog and text fields

Consume a custom control in a scope's onAction or a page subscription before defaults. For example, a preview scope may own rightStick change events and marshal its analog value to the existing native camera API. Also handle cancel/active=false so a held preview operation stops on focus loss. A leftStick handler receives its navigation actions as well as axis updates, allowing it to suppress shared navigation in that scope.

Text fields retain normal CEF focus and Meridian's existing text-input lease. Controller directions do not edit text or unexpectedly move focus while editing. Cancel leaves the edit context first. **A physical keyboard is still required for free-text entry.** This release does not add an on-screen keyboard.

## Runtime configuration

Optional [Controller] keys in Data/SKSE/Plugins/MeridianUI.ini:

| Key | Default | Accepted range |
|---|---|---|
| Enabled | true | Boolean; false overrides view opt-in |
| DeadZone | 0.25 | 0.05 to 0.90 normalized |
| ExitDeadZone | 0.18 | 0 to 0.89, constrained below DeadZone |
| CursorSpeed | 900 | 100 to 3000 reference pixels/sec, scaled by view height / 1080 |
| RepeatDelay | 0.350 | 0.1 to 2 seconds |
| RepeatInterval | 0.090 | 0.03 to 0.5 seconds |
| GlyphFamily | Xbox | Xbox, PlayStation, Generic |
| TraceInput | false | Diagnostic IDs/values in MeridianUI.log |

The engine is the input source. Meridian does not independently poll Windows controllers. The initial hardware target is Skyrim's active standard gamepad on SE/AE. Xbox/XInput is the reference device. PlayStation through a translation configuration and any direct device backend need their own recorded hardware test. VR, multiple players, gyro, haptics and touchpads are outside this interface.

Controller input is captured synchronously while a configured, ready, visible view owns focus. Unhandled page actions stay captured. Connection events and necessary gameplay-owned releases continue downstream. No-focus input and existing non-opted-in views keep their device behavior. Protection covers the engine dispatch chain; it is not a claim to intercept arbitrary external OS polling.

## SDK and validation

The InputAPI/InputDllLoader headers are covered by the existing MIT license in MeridianUIAPI. The injected helper is part of the Meridian runtime under the repository's existing implementation license; consumers use it from the runtime rather than copying its source. See [licensing](../LICENSING.md).

Run native CTest, Node helper tests and the browser navigation suite, then complete the [runtime gate](testing/MERIDIAN_CONTROLLER_RUNTIME_GATE.md) using MeridianInputTest. Test both paused and unpaused views, two competing views, shortcuts, hotkeys, text entry, disconnect, alt-tab, reload, destruction, and shutdown. Each actual consumer still needs its own UI/action tests after adoption.
