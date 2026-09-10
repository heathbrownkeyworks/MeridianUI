// Standalone diagnostic consumer. No Horde/Romantasy/Tailor dependencies.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "MeridianUIAPI/ViewDllLoader.h"
#include "MeridianUIAPI/InputDllLoader.h"
#include "RuntimeCompatibility.h"
#include <atomic>
#include <string>
using namespace Meridian::UI;
namespace
{
    constexpr auto Name = "MeridianInputTest";
    View::IViewAPI* views = nullptr;
    Input::IInputAPI* input = nullptr;
    View::ViewHandle handles[2]{};
    std::atomic<View::ViewHandle> active{0};
    bool pause = true;
    bool dataLoaded = false;
    void Close()
    {
        auto handle = active.exchange(0);
        if (handle && views)
        {
            views->Unfocus(handle);
            views->Hide(handle);
        }
    }
    void Open(int index)
    {
        if (!views || !handles[index] || !views->IsReady(handles[index]))
            return;
        Close();
        const auto handle = handles[index];
        views->Show(handle);
        const auto result = views->TryFocus(handle, pause ? View::FocusMode::PauseGame : View::FocusMode::Unpaused);
        if (result == View::FocusResult::Granted || result == View::FocusResult::AlreadyFocused)
            active = handle;
        else
            views->Hide(handle);
    }
    void __cdecl OnShortcut(Input::ShortcutHandle, void*)
    {
        Open(0);
    }
    void Create(int index);
    void __cdecl OnReady(View::ViewHandle handle)
    {
        Input::ViewInputConfig config{};
        config.enabled = 1;
        if (input)
            input->ConfigureView(handle, &config);
        views->RegisterListener(handle, "inputTestClose", [](const char*) {
            SKSE::GetTaskInterface()->AddTask(Close);
        });
        views->RegisterListener(handle, "inputTestCommand", [](const char* payload) {
            const std::string command = payload ? payload : "";
            SKSE::GetTaskInterface()->AddTask([command]() {
                if (command == "paused" || command == "unpaused")
                {
                    pause = command == "paused";
                    Open(0);
                }
                else if (command == "main")
                    Open(0);
                else if (command == "secondary")
                    Open(1);
                else if (command == "compete")
                {
                    auto other = active.load() == handles[0] ? handles[1] : handles[0];
                    const auto result = views->TryFocus(other, View::FocusMode::Unpaused);
                    if (auto current = active.load())
                    {
                        const auto script = "document.querySelector('#result').textContent='Competing focus result: " + std::to_string(static_cast<unsigned>(result)) + " (2 = Busy)';";
                        views->ExecuteJavaScript(current, script.c_str());
                    }
                }
                else if (command == "reload")
                {
                    if (auto current = active.load())
                        views->ExecuteJavaScript(current, "location.reload();");
                }
                else if (command == "recreate")
                {
                    Close();
                    for (auto& h : handles)
                    {
                        if (h)
                            views->DestroyView(h);
                        h = 0;
                    }
                    Create(0);
                    Create(1);
                }
                else if (command == "close")
                    Close();
            });
        });
    }
    void Create(int index)
    {
        View::ViewCreateInfo info;
        info.ownerName = "meridianinputtest";
        info.viewName = index ? "secondary" : "main";
        info.startUrl = index ? "mod://meridianinputtest/index.html?secondary" : "mod://meridianinputtest/index.html";
        info.onDOMReady = OnReady;
        handles[index] = views->CreateView(&info);
        if (index == 0 && input && handles[0])
        {
            Input::ShortcutInfo shortcut;
            shortcut.button = Input::Control::Start;
            shortcut.modifier = Input::Control::LeftShoulder;
            shortcut.callback = OnShortcut;
            Input::ShortcutHandle id = 0;
            const auto result = input->RegisterShortcut(handles[0], &shortcut, &id);
            spdlog::info("LB+Start registration result={} handle={}", static_cast<unsigned>(result), id);
            Input::ShortcutHandle duplicate = 0;
            const auto conflict = input->RegisterShortcut(handles[0], &shortcut, &duplicate);
            spdlog::info("duplicate shortcut result={} (3 = Conflict), handle={}", static_cast<unsigned>(conflict), duplicate);
        }
    }
    void Initialize()
    {
        if (!dataLoaded || !views || handles[0])
            return;
        Create(0);
        Create(1);
    }
    class Keys final : public RE::BSTEventSink<RE::InputEvent*>
    {
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
        {
            for (auto* event = events ? *events : nullptr; event; event = event->next)
            {
                if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton)
                    continue;
                auto button = event->AsButtonEvent();
                if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->IsDown() && button->GetIDCode() == 0x44)
                    SKSE::GetTaskInterface()->AddTask([]() {if(active)Close();else Open(0); });
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } keys;
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version =
    Meridian::RuntimeCompatibility::MakePluginVersionData(1, Name);
extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info)
{
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = Name;
    info->version = 1;
    return !skse->IsEditor() && skse->RuntimeVersion() >= SKSE::RUNTIME_SSE_1_5_39;
}
SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    if (skse->IsEditor())
        return false;
    SKSE::Init(skse);
    if (auto dir = SKSE::log::log_directory())
    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "MeridianInputTest.log").string(), true);
        auto log = std::make_shared<spdlog::logger>(Name, sink);
        spdlog::set_default_logger(log);
        log->flush_on(spdlog::level::info);
    }
    return SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kInputLoaded)
        {
            Settings settings{};
            views = View::Query(&settings, Name);
            input = Input::Query(&settings, Name);
            RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&keys);
            spdlog::info("View/1={} Input/1={}; F10 or LB+Start opens fixture", views != nullptr, input != nullptr);
            Initialize();
        }
        else if (message->type == SKSE::MessagingInterface::kDataLoaded)
        {
            dataLoaded = true;
            Initialize();
        }
        else if (message->type == SKSE::MessagingInterface::kPreLoadGame)
            Close();
    });
}
