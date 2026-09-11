#include "Services/ControllerInputService.h"
#include "Controllers/ViewAPIController.h"
#include "CEF/DefaultBrowser.h"
#include "Config/IniConfig.h"
#include "Menus/FocusArbiter.h"
#include <chrono>

namespace Meridian::Services
{
    using namespace Meridian::Input;
    using UI::Input::Result;
    using UI::Input::ViewHandle;
    using UI::Input::ShortcutHandle;
    using json = nlohmann::json;
    namespace
    {
        double Now()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        bool Foreground()
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &pid);
            return pid == GetCurrentProcessId();
        }
        bool CanOpenShortcut()
        {
            auto ui = RE::UI::GetSingleton();
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!ui || !player || !player->Is3DLoaded() || ui->GameIsPaused())
                return false;
            for (const auto* name : {"Console", "Main Menu", "Loading Menu", "Dialogue Menu", "RaceSex Menu", "Fader Menu"})
                if (ui->IsMenuOpen(name))
                    return false;
            return true;
        }
        Control ButtonControl(RE::ButtonEvent* button)
        {
            // CommonLib maps the active gamepad family into stable SKSE macros,
            // including the engine's synthetic trigger IDs.
            auto id = SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode());
            return FromSKSEKeycode(id);
        }
        void SeedGamepad(ControllerState& state, RE::BSInputDeviceManager* manager)
        {
            if (!manager)
                return;
            auto gamepad = skyrim_cast<RE::BSWin32GamepadDevice*>(manager->GetGamepad());
            if (!gamepad)
                return; // direct non-XInput devices require neutral event transitions
            const auto& data = gamepad->GetRuntimeData().currentState.gamepad;
            constexpr unsigned masks[]{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 4096, 8192, 16384, 32768};
            for (unsigned i = 0; i < 14; ++i)
                state.Seed(FromXInputButton(masks[i]), (data.buttons & masks[i]) ? 1.0f : 0.0f);
            state.Seed(Control::LeftTrigger, data.leftTrigger / 255.0f);
            state.Seed(Control::RightTrigger, data.rightTrigger / 255.0f);
            state.Seed(Control::LeftStick, data.thumbLX / 32767.0f, data.thumbLY / 32767.0f);
            state.Seed(Control::RightStick, data.thumbRX / 32767.0f, data.thumbRY / 32767.0f);
        }
        json Bindings(const UI::Input::ViewInputConfig& config)
        {
            json result = json::object();
            for (unsigned i = 0; i < UI::Input::BINDING_COUNT; ++i)
                result[ActionName(static_cast<Action>(i + 1))] = ControlName(config.bindings[i]);
            return result;
        }
    }
    ControllerInputService& ControllerInputService::GetSingleton()
    {
        static ControllerInputService service;
        return service;
    }
    Result ControllerInputService::Configure(ViewHandle view, const UI::Input::ViewInputConfig* config)
    {
        if (!config || config->structSize < sizeof(*config) || !ValidConfig(*config))
            return Result::InvalidArgument;
        auto browser = Controllers::ViewAPIController::GetSingleton().GetBrowserForInput(view);
        if (!browser)
            return Result::InvalidView;
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown)
                return Result::ShuttingDown;
            auto& entry = m_views[view];
            entry.browser = browser;
            entry.config = *config;
            entry.mode = config->defaultMode;
            entry.stateDirty = true;
        }
        browser->InvalidateControllerInput();
        if (!Controllers::ViewAPIController::GetSingleton().IsValid(view))
        {
            RemoveView(view);
            return Result::InvalidView;
        }
        return Result::Ok;
    }
    Result ControllerInputService::GetState(ViewHandle view, UI::Input::InputState* out) const
    {
        if (!out || out->structSize < sizeof(*out))
            return Result::InvalidArgument;
        if (!Controllers::ViewAPIController::GetSingleton().IsValid(view))
            return Result::InvalidView;
        std::lock_guard lock(m_mutex);
        if (m_shutdown)
            return Result::ShuttingDown;
        *out = {};
        out->connected = m_connected;
        out->generation = m_state.Generation();
        out->glyphFamily = m_state.tuning.glyphFamily;
        if (auto it = m_views.find(view); it != m_views.end())
        {
            out->enabled = it->second.config.enabled && m_state.tuning.enabled;
            out->mode = it->second.mode;
            out->capturing = m_state.Owner() == view;
        }
        return Result::Ok;
    }
    Result ControllerInputService::RegisterShortcut(ViewHandle view, const UI::Input::ShortcutInfo* info, ShortcutHandle* out)
    {
        if (out)
            *out = 0;
        if (!out || !info || info->structSize < sizeof(*info) || !ValidShortcut(*info))
            return Result::InvalidArgument;
        auto browser = Controllers::ViewAPIController::GetSingleton().GetBrowserForInput(view);
        if (!browser)
            return Result::InvalidView;
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown)
                return Result::ShuttingDown;
            for (const auto& [id, shortcut] : m_shortcuts)
            {
                (void)id;
                if (ShortcutConflict(*info, shortcut.info))
                    return Result::Conflict;
            }
            *out = m_nextShortcut++;
            m_shortcuts.emplace(*out, Shortcut{view, *info});
        }
        if (!Controllers::ViewAPIController::GetSingleton().IsValid(view))
        {
            RemoveView(view);
            *out = 0;
            return Result::InvalidView;
        }
        return Result::Ok;
    }
    void ControllerInputService::UnregisterShortcut(ShortcutHandle handle)
    {
        std::lock_guard lock(m_mutex);
        m_shortcuts.erase(handle);
    }
    void ControllerInputService::RemoveView(ViewHandle view)
    {
        std::lock_guard lock(m_mutex);
        m_views.erase(view);
        std::erase_if(m_shortcuts, [view](const auto& item) { return item.second.view == view; });
        if (m_state.Owner() == view)
            m_state.SetOwner(0);
    }
    void ControllerInputService::PageRequest(ViewHandle view, const std::string& payload)
    {
        if (payload.size() > 512)
            return;
        auto request = json::parse(payload, nullptr, false);
        if (!request.is_object() || !request.contains("op") || !request["op"].is_string() ||
            !request.contains("page") || !request["page"].is_string())
            return;
        auto page = request["page"].get<std::string>();
        if (page.empty() || page.size() > 80)
            return;
        auto browser = Controllers::ViewAPIController::GetSingleton().GetBrowserForInput(view);
        if (!browser)
            return;
        bool resetPointer = false;
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown)
                return;
            auto& entry = m_views[view];
            entry.browser = browser;
            const auto op = request["op"].get<std::string>();
            if (op == "ready")
            {
                entry.page = page;
                entry.stateDirty = true;
            }
            else if (entry.page == page && op == "mode" && request.contains("mode") && request["mode"].is_string())
            {
                auto mode = request["mode"].get<std::string>();
                if (mode != "navigation" && mode != "cursor")
                    return;
                if (!entry.config.enabled || (mode == "cursor" && !entry.config.allowCursor))
                    return;
                entry.mode = mode == "cursor" ? Mode::Cursor : Mode::Navigation;
                entry.stateDirty = true;
                resetPointer = true;
            }
        }
        if (resetPointer)
            browser->InvalidateControllerInput();
        if (!Controllers::ViewAPIController::GetSingleton().IsValid(view))
            RemoveView(view);
    }
    void ControllerInputService::CancelBrowser(CEF::DefaultBrowser* browser, bool navigation)
    {
        struct Notice
        {
            std::shared_ptr<CEF::DefaultBrowser> browser;
            std::string packet;
            std::uint64_t epoch;
        };
        std::vector<Notice> notices;
        {
            std::lock_guard lock(m_mutex);
            for (auto& [view, entry] : m_views)
            {
                auto strong = entry.browser.lock();
                if (strong.get() != browser)
                    continue;
                if (m_state.Owner() == view)
                    m_state.SetOwner(0);
                entry.stateDirty = true;
                entry.epoch = 0;
                if (!entry.page.empty())
                {
                    json packet = {{"version", 1}, {"page", entry.page}, {"generation", m_state.Generation()}, {"sequence", ++m_sequence}, {"active", false}, {"reset", true}, {"events", json::array()}};
                    notices.push_back({strong, packet.dump(), strong->ControllerEpoch()});
                }
                if (navigation)
                    entry.page.clear();
            }
            m_drawCursor = true;
        }
        for (auto& n : notices)
            n.browser->SendControllerPacket(std::move(n.packet), n.epoch);
    }
    void ControllerInputService::Shutdown()
    {
        std::lock_guard lock(m_mutex);
        m_shutdown = true;
        m_views.clear();
        m_shortcuts.clear();
        m_state.SetOwner(0);
        m_state.ResetDevice();
        m_drawCursor = true;
    }
    std::unordered_set<RE::InputEvent*> ControllerInputService::Route(RE::InputEvent* head)
    {
        struct Candidate
        {
            ViewHandle view;
            std::shared_ptr<CEF::DefaultBrowser> browser;
            std::uint64_t epoch;
            bool eligible;
        };
        struct Delivery
        {
            std::shared_ptr<CEF::DefaultBrowser> browser;
            std::uint64_t epoch;
            std::string packet;
        };
        std::vector<Candidate> candidates;
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown)
                return {};
            for (auto& [view, entry] : m_views)
                if (auto browser = entry.browser.lock())
                    candidates.push_back({view, browser, browser->ControllerEpoch(), false});
        }
        const bool foreground = Foreground();
        const bool anyFocus = Menus::FocusArbiter::GetSingleton().HasOwner();
        const bool canOpen = foreground && !anyFocus && CanOpenShortcut();
        auto manager = RE::BSInputDeviceManager::GetSingleton();
        const bool connected = manager && manager->IsGamepadConnected() && manager->IsGamepadEnabled();
        for (auto& candidate : candidates)
            candidate.eligible = foreground && candidate.browser->IsBrowserFocused() &&
                                 candidate.browser->IsBrowserVisible() && candidate.browser->IsPageLoaded();
        std::unordered_set<RE::InputEvent*> consumed;
        std::vector<Delivery> deliveries;
        std::vector<int> clicks;
        std::shared_ptr<CEF::DefaultBrowser> pointerBrowser;
        std::shared_ptr<CEF::DefaultBrowser> cancelledBrowser;
        std::uint64_t pointerEpoch = 0;
        float pointerDX = 0, pointerDY = 0;
        bool releasePointer = false;
        ShortcutHandle fireShortcut = 0;
        const double now = Now();
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown)
                return {};
            if (!m_initialized)
            {
                m_initialized = true;
                const auto& config = Config::LoadIniOverrides();
                m_state.tuning = config.controller;
            }
            const bool connectionChanged = connected != m_connected;
            if (connectionChanged)
            {
                m_connected = connected;
                m_state.ResetDevice();
                if (connected)
                    SeedGamepad(m_state, manager);
            }
            const auto previousOwner = m_state.Owner();
            ViewHandle owner = 0;
            Entry* active = nullptr;
            for (auto& c : candidates)
            {
                auto it = m_views.find(c.view);
                if (it == m_views.end())
                    continue;
                auto& entry = it->second;
                if (entry.config.enabled && m_state.tuning.enabled && c.eligible && connected && !entry.page.empty())
                {
                    owner = c.view;
                    active = &entry;
                    pointerBrowser = c.browser;
                    pointerEpoch = c.epoch;
                    if (entry.epoch != c.epoch)
                    {
                        m_state.SetOwner(0);
                        if (previousOwner == owner)
                            SeedGamepad(m_state, manager);
                        entry.epoch = c.epoch;
                        entry.stateDirty = true;
                    }
                    break;
                }
            }
            bool reset = m_state.SetOwner(owner);
            if (previousOwner && previousOwner != owner)
                for (auto& c : candidates)
                    if (c.view == previousOwner)
                        cancelledBrowser = c.browser;
            const auto generation = m_state.Generation();
            const double dt = std::clamp(m_lastTime == 0 ? 0.0 : now - m_lastTime, 0.0, 0.05);
            m_lastTime = now;
            if (m_state.tuning.trace)
            {
                ++m_traceDispatches;
                if (!head)
                    ++m_traceEmpty;
                if (now - m_lastTrace >= 1.0)
                {
                    LOG_INFO("Controller dispatch cadence: batches={} empty={} connected={} owner={} dt={}",
                             m_traceDispatches,
                             m_traceEmpty,
                             connected,
                             owner,
                             dt);
                    m_traceDispatches = m_traceEmpty = 0;
                    m_lastTrace = now;
                }
            }
            json events = json::array();
            bool axesChanged = false;
            auto emit = [&](Control control, Action action, Phase phase, float x = 0, float y = 0) {
                if (active && phase != Phase::None)
                    events.push_back({{"control", ControlName(control)}, {"action", ActionName(action)}, {"phase", PhaseName(phase)}, {"value", x}, {"x", x}, {"y", y}});
            };
            for (auto* event = head; event; event = event->next)
            {
                if (event->GetDevice() != RE::INPUT_DEVICE::kGamepad)
                {
                    bool activity = false;
                    if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove)
                    {
                        auto mouse = event->AsMouseMoveEvent();
                        activity = mouse->mouseInputX != 0 || mouse->mouseInputY != 0;
                    }
                    else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton)
                        activity = event->AsButtonEvent()->IsDown();
                    if (activity && m_gamepadPresentation)
                    {
                        m_gamepadPresentation = false;
                        if (active)
                            active->stateDirty = true;
                        releasePointer = true;
                        clicks.clear();
                    }
                    continue;
                }
                if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kDeviceConnect)
                    continue;
                if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton)
                {
                    auto button = event->AsButtonEvent();
                    auto control = ButtonControl(button);
                    if (m_state.tuning.trace)
                        LOG_INFO("Controller button id={} value={} held={}", button->GetIDCode(), button->Value(), button->HeldDuration());
                    bool force = false;
                    if (control != Control::None && button->Value() > 0 && !m_state.Held(control) &&
                        m_state.Armed(control) && connected && canOpen && m_state.tuning.enabled && !m_shortcutPending)
                    {
                        for (auto& [id, shortcut] : m_shortcuts)
                            if (shortcut.info.button == control && (shortcut.info.modifier == Control::None ||
                                                                    (m_state.Armed(shortcut.info.modifier) && m_state.Held(shortcut.info.modifier))))
                            {
                                fireShortcut = id;
                                force = true;
                                m_shortcutPending = true;
                                break;
                            }
                    }
                    auto transition = m_state.Button(control, button->Value(), force);
                    if (transition.consume || (active && control == Control::None))
                        consumed.insert(event);
                    if (transition.phase == Phase::Press && !m_gamepadPresentation)
                    {
                        m_gamepadPresentation = true;
                        if (active)
                            active->stateDirty = true;
                    }
                    if (!active || transition.phase == Phase::None)
                        continue;
                    auto action = BindingAction(active->config, control);
                    // Navigation repeats are emitted once by the clock below.
                    if (action >= Action::Up && action <= Action::Right)
                    {
                        if (transition.phase == Phase::Release)
                            emit(control, action, Phase::Release, 0);
                        if (transition.phase == Phase::Press && active->mode == Mode::Cursor)
                        {
                            active->mode = Mode::Navigation;
                            active->stateDirty = true;
                            releasePointer = true;
                            clicks.clear();
                        }
                        continue;
                    }
                    if (action == Action::ToggleCursor && transition.phase == Phase::Press && active->config.allowCursor)
                    {
                        active->mode = active->mode == Mode::Navigation ? Mode::Cursor : Mode::Navigation;
                        active->stateDirty = true;
                        releasePointer = true;
                        clicks.clear();
                        m_state.Capture(active->config.bindings[static_cast<unsigned>(Action::Accept) - 1]);
                        continue;
                    }
                    if (active->mode == Mode::Cursor && action == Action::Accept)
                    {
                        if (transition.phase == Phase::Press)
                            clicks.push_back(1);
                        if (transition.phase == Phase::Release)
                            clicks.push_back(0);
                    }
                    else
                        emit(control, action, transition.phase, button->Value());
                }
                else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick)
                {
                    auto stick = event->AsThumbstickEvent();
                    auto control = stick->IsLeft() ? Control::LeftStick : Control::RightStick;
                    if (m_state.tuning.trace)
                        LOG_INFO("Controller stick id={} x={} y={}", stick->GetIDCode(), stick->xValue, stick->yValue);
                    auto transition = m_state.Stick(control, stick->xValue, stick->yValue);
                    if (transition.consume)
                        consumed.insert(event);
                    if (transition.phase != Phase::None)
                    {
                        axesChanged = true;
                        if (!m_gamepadPresentation && (m_state.X(control) != 0 || m_state.Y(control) != 0))
                        {
                            m_gamepadPresentation = true;
                            if (active)
                                active->stateDirty = true;
                        }
                        // Continuous state below carries the latest axes once per dispatch.
                    }
                }
            }
            if (active)
            {
                if (auto direction = m_state.Repeat(now, active->mode == Mode::Navigation, &active->config); direction != Control::None)
                {
                    const auto binding = active->config.bindings[Index(direction) - 1];
                    const auto source = m_state.Held(binding) ? binding : Control::LeftStick;
                    emit(source, static_cast<Action>(direction), m_state.InitialRepeat() ? Phase::Press : Phase::Repeat, 1);
                }
                for (auto c : {Control::LeftStick, Control::RightStick})
                    if (axesChanged || m_state.X(c) != 0 || m_state.Y(c) != 0)
                        emit(c, Action::None, Phase::Change, m_state.X(c), m_state.Y(c));
                if (active->mode == Mode::Cursor && m_gamepadPresentation)
                {
                    pointerDX = m_state.X(Control::LeftStick) * m_state.tuning.cursorSpeed * static_cast<float>(dt);
                    pointerDY = -m_state.Y(Control::LeftStick) * m_state.tuning.cursorSpeed * static_cast<float>(dt);
                }
            }
            if (events.size() > 128)
            {
                events = json::array();
                m_state.RequireNeutral();
                SeedGamepad(m_state, manager);
                clicks.clear();
                pointerDX = pointerDY = 0;
                releasePointer = true;
                reset = true;
            }
            for (auto& c : candidates)
            {
                auto it = m_views.find(c.view);
                if (it == m_views.end() || it->second.page.empty())
                    continue;
                auto& entry = it->second;
                bool capturing = c.view == owner;
                if (!entry.stateDirty && !connectionChanged && !reset && (!capturing || events.empty()))
                    continue;
                json packet = {{"version", 1}, {"page", entry.page}, {"generation", generation}, {"sequence", ++m_sequence}, {"active", capturing}, {"reset", reset}, {"enabled", entry.config.enabled && m_state.tuning.enabled}, {"connected", connected}, {"mode", entry.mode == Mode::Cursor ? "cursor" : "navigation"}, {"device", m_gamepadPresentation ? "gamepad" : "keyboardMouse"}, {"glyphFamily", m_state.tuning.glyphFamily == UI::Input::GlyphFamily::PlayStation ? "playstation" : m_state.tuning.glyphFamily == UI::Input::GlyphFamily::Xbox ? "xbox"
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             : "generic"},
                               {"bindings", Bindings(entry.config)},
                               {"dt", dt},
                               {"events", capturing ? events : json::array()}};
                deliveries.push_back({c.browser, c.epoch, packet.dump()});
                entry.stateDirty = false;
            }
            m_drawCursor = !(active && m_gamepadPresentation && active->mode == Mode::Navigation);
        }
        // No browser, CEF or consumer call occurs under the service mutex.
        if (cancelledBrowser)
        {
            cancelledBrowser->InvalidateControllerInput();
            for (auto& d : deliveries)
                if (d.browser == cancelledBrowser)
                    d.epoch = cancelledBrowser->ControllerEpoch();
        }
        if (pointerBrowser)
        {
            if (releasePointer)
                pointerBrowser->ReleaseControllerPointer();
            if (pointerDX != 0 || pointerDY != 0)
                pointerBrowser->ControllerPointer(pointerDX, pointerDY, -1, pointerEpoch);
            for (auto click : clicks)
                pointerBrowser->ControllerPointer(0, 0, click, pointerEpoch);
        }
        for (auto& d : deliveries)
            d.browser->SendControllerPacket(std::move(d.packet), d.epoch);
        if (fireShortcut)
        {
            auto tasks = SKSE::GetTaskInterface();
            if (tasks)
                tasks->AddTask([this, fireShortcut]() {
                    UI::Input::ShortcutInfo info{};
                    ViewHandle view = 0;
                    {
                        std::lock_guard lock(m_mutex);
                        m_shortcutPending = false;
                        auto it = m_shortcuts.find(fireShortcut);
                        if (m_shutdown || !m_connected || !m_state.tuning.enabled || it == m_shortcuts.end())
                            return;
                        info = it->second.info;
                        view = it->second.view;
                    }
                    if (Foreground() && CanOpenShortcut() && !Menus::FocusArbiter::GetSingleton().HasOwner() &&
                        Controllers::ViewAPIController::GetSingleton().IsValid(view))
                        info.callback(fireShortcut, info.userData);
                });
            else
            {
                std::lock_guard lock(m_mutex);
                m_shortcutPending = false;
            }
        }
        return consumed;
    }
}
