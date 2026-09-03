#include "Controllers/ViewAPIController.h"

#include "CEF/DefaultBrowser.h"
#include "Controllers/PublicAPIController.h"
#include "Controllers/ViewBridgeScripts.h"
#include "Menus/FocusArbiter.h"
#include "Scheme/ModSchemePath.h"

#include <array>
#include <algorithm>
#include <bcrypt.h>
#include <cctype>

namespace Meridian::Controllers
{
    namespace
    {
        constexpr char kNativeObject[] = "MeridianViewNative";
        constexpr char kDispatchFunction[] = "dispatch";
        constexpr char kReadyListener[] = "__meridian_dom_ready";
        constexpr char kTextInputListener[] = "__meridian_text_input";
        std::string ToLower(std::string_view a_value)
        {
            std::string result(a_value);
            for (char& c : result)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return result;
        }
    }

    Meridian::UI::View::ViewHandle __cdecl ViewAPIController::CreateView(
        const Meridian::UI::View::ViewCreateInfo* a_info)
    {
        using namespace Meridian::UI::View;

        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < VIEW_CREATE_INFO_MIN_SIZE_1 ||
            a_info->ownerName == nullptr ||
            a_info->viewName == nullptr ||
            a_info->startUrl == nullptr ||
            !IsSafeName(a_info->ownerName) ||
            !IsSafeName(a_info->viewName))
        {
            return INVALID_VIEW_HANDLE;
        }

        const auto parsedUrl = Meridian::Scheme::ParseModUrl(a_info->startUrl);
        if (!parsedUrl.has_value() || ToLower(parsedUrl->modName) != ToLower(a_info->ownerName))
        {
            spdlog::error("{}: refusing view whose mod:// host does not match owner '{}'", NameOf(ViewAPIController), a_info->ownerName);
            return INVALID_VIEW_HANDLE;
        }

        const std::string browserName = fmt::format("Meridian.View/{}/{}", a_info->ownerName, a_info->viewName);
        {
            std::lock_guard lock(m_mutex);
            if (m_ownedBrowserNames.contains(browserName))
            {
                spdlog::warn("{}: duplicate view name '{}'", NameOf(ViewAPIController), browserName);
                return INVALID_VIEW_HANDLE;
            }
            m_ownedBrowserNames.insert(browserName);
        }

        const auto viewHandle = m_nextHandle.fetch_add(1, std::memory_order_relaxed);
        const std::string token = GenerateToken();
        if (token.empty())
        {
            std::lock_guard lock(m_mutex);
            m_ownedBrowserNames.erase(browserName);
            return INVALID_VIEW_HANDLE;
        }

        Meridian::JS::JSFuncInfo dispatchInfo{};
        dispatchInfo.objectName = kNativeObject;
        dispatchInfo.funcName = kDispatchFunction;
        dispatchInfo.callbackData.callback = &ViewAPIController::Dispatch;
        dispatchInfo.callbackData.executeInGameThread = false;
        Meridian::JS::JSFuncInfo* callbacks[] = {&dispatchInfo};

        Meridian::UI::BrowserSettings browserSettings{};
        browserSettings.frameRate = std::clamp(a_info->frameRate, 1, 240);

        Meridian::CEF::IBrowser* browserInterface = nullptr;
        auto& publicController = PublicAPIController::GetSingleton();
        const auto browserHandle = publicController.AddOrGetBrowser(
            browserName.c_str(),
            callbacks,
            static_cast<std::uint32_t>(std::size(callbacks)),
            a_info->startUrl,
            &browserSettings,
            browserInterface);

        auto* browserRaw = dynamic_cast<Meridian::CEF::DefaultBrowser*>(browserInterface);
        if (browserHandle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle || browserRaw == nullptr)
        {
            if (browserHandle != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
            {
                publicController.ReleaseBrowserHandle(browserHandle);
            }
            std::lock_guard lock(m_mutex);
            m_ownedBrowserNames.erase(browserName);
            return INVALID_VIEW_HANDLE;
        }

        const auto browser = browserRaw->shared_from_this();

        auto entry = std::make_shared<ViewEntry>();
        entry->handle = viewHandle;
        entry->browserHandle = browserHandle;
        entry->browser = browser;
        entry->browserName = browserName;
        entry->token = token;
        entry->onDOMReady = a_info->onDOMReady;
        {
            std::lock_guard lock(m_mutex);
            if (m_isShuttingDown.load(std::memory_order_acquire))
            {
                m_ownedBrowserNames.erase(browserName);
                publicController.ReleaseBrowserHandle(browserHandle);
                return INVALID_VIEW_HANDLE;
            }
            m_views.emplace(viewHandle, entry);
            m_tokenToView.emplace(token, viewHandle);
        }

        browser->SetBrowserVisible(a_info->initiallyVisible);
        const auto bootstrap = ViewBridgeScripts::BuildBootstrap(token);
        browser->AddPersistentJavaScript("00-bootstrap", bootstrap.c_str());

        spdlog::info("{}: created {} as handle {}", NameOf(ViewAPIController), browserName, viewHandle);
        return viewHandle;
    }

    void __cdecl ViewAPIController::DestroyView(Meridian::UI::View::ViewHandle a_view)
    {
        std::shared_ptr<ViewEntry> entry;
        {
            std::lock_guard lock(m_mutex);
            const auto it = m_views.find(a_view);
            if (it == m_views.end())
            {
                return;
            }
            entry = it->second;
            m_tokenToView.erase(entry->token);
            m_views.erase(it);
            m_ownedBrowserNames.erase(entry->browserName);
        }

        if (entry->browser != nullptr)
        {
            entry->browser->SetBrowserFocused(false);
            entry->browser->SetBrowserVisible(false);
        }
        ReleaseBrowser(entry->browserHandle);
    }

    bool __cdecl ViewAPIController::IsValid(Meridian::UI::View::ViewHandle a_view) const
    {
        return GetEntry(a_view) != nullptr;
    }

    bool __cdecl ViewAPIController::IsReady(Meridian::UI::View::ViewHandle a_view) const
    {
        const auto entry = GetEntry(a_view);
        return entry != nullptr && entry->browser != nullptr && entry->browser->IsPageLoaded();
    }

    bool __cdecl ViewAPIController::RegisterListener(
        Meridian::UI::View::ViewHandle a_view,
        const char* a_name,
        Meridian::UI::View::ListenerCallback a_callback)
    {
        if (a_name == nullptr || a_callback == nullptr || !IsSafeName(a_name))
        {
            return false;
        }

        const auto entry = GetEntry(a_view);
        if (entry == nullptr || entry->browser == nullptr)
        {
            return false;
        }
        {
            std::lock_guard lock(m_mutex);
            const auto it = m_views.find(a_view);
            if (it == m_views.end())
            {
                return false;
            }
            it->second->listeners[std::string(a_name)] = a_callback;
        }

        const auto script = ViewBridgeScripts::BuildListener(a_name);
        const auto scriptKey = fmt::format("10-listener-{}", a_name);
        entry->browser->AddPersistentJavaScript(scriptKey.c_str(), script.c_str());
        return true;
    }

    bool __cdecl ViewAPIController::ExecuteJavaScript(
        Meridian::UI::View::ViewHandle a_view,
        const char* a_script)
    {
        const auto entry = GetEntry(a_view);
        if (entry == nullptr || entry->browser == nullptr || a_script == nullptr)
        {
            return false;
        }
        entry->browser->ExecuteJavaScript(a_script);
        return true;
    }

    bool __cdecl ViewAPIController::Show(Meridian::UI::View::ViewHandle a_view)
    {
        const auto entry = GetEntry(a_view);
        if (entry == nullptr || entry->browser == nullptr)
        {
            return false;
        }
        entry->browser->SetBrowserVisible(true);
        return true;
    }

    bool __cdecl ViewAPIController::Hide(Meridian::UI::View::ViewHandle a_view)
    {
        const auto entry = GetEntry(a_view);
        if (entry == nullptr || entry->browser == nullptr)
        {
            return false;
        }
        entry->browser->SetBrowserVisible(false);
        return true;
    }

    Meridian::UI::View::FocusResult __cdecl ViewAPIController::TryFocus(
        Meridian::UI::View::ViewHandle a_view,
        Meridian::UI::View::FocusMode a_mode)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return Meridian::UI::View::FocusResult::ShuttingDown;
        }
        if (a_mode != Meridian::UI::View::FocusMode::Unpaused &&
            a_mode != Meridian::UI::View::FocusMode::PauseGame)
        {
            return Meridian::UI::View::FocusResult::InvalidView;
        }

        const auto entry = GetEntry(a_view);
        if (entry == nullptr || entry->browser == nullptr)
        {
            return Meridian::UI::View::FocusResult::InvalidView;
        }
        return entry->browser->TryViewFocus(a_mode);
    }

    void __cdecl ViewAPIController::Unfocus(Meridian::UI::View::ViewHandle a_view)
    {
        const auto entry = GetEntry(a_view);
        if (entry != nullptr && entry->browser != nullptr)
        {
            entry->browser->SetBrowserFocused(false);
        }
    }

    bool __cdecl ViewAPIController::HasFocus(Meridian::UI::View::ViewHandle a_view) const
    {
        const auto entry = GetEntry(a_view);
        return entry != nullptr && entry->browser != nullptr && entry->browser->IsBrowserFocused();
    }

    bool __cdecl ViewAPIController::HasAnyFocus() const
    {
        return Meridian::Menus::FocusArbiter::GetSingleton().HasOwner();
    }

    void ViewAPIController::BeginShutdown()
    {
        if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        std::lock_guard lock(m_mutex);
        m_tokenToView.clear();
        m_views.clear();
        m_ownedBrowserNames.clear();
    }

    bool ViewAPIController::IsShuttingDown() const
    {
        return m_isShuttingDown.load(std::memory_order_acquire);
    }

    void ViewAPIController::Dispatch(const char** a_args, int a_argsCount)
    {
        if (a_args == nullptr || a_argsCount < 3)
        {
            return;
        }

        const std::string token = DecodeStringArgument(a_args[0]);
        const std::string listenerName = DecodeStringArgument(a_args[1]);
        const std::string payload = DecodeStringArgument(a_args[2]);

        Meridian::UI::View::ListenerCallback listener = nullptr;
        Meridian::UI::View::DOMReadyCallback ready = nullptr;
        std::shared_ptr<Meridian::CEF::DefaultBrowser> textInputBrowser;
        Meridian::UI::View::ViewHandle handle = Meridian::UI::View::INVALID_VIEW_HANDLE;
        auto& controller = GetSingleton();
        {
            std::lock_guard lock(controller.m_mutex);
            const auto tokenIt = controller.m_tokenToView.find(token);
            if (tokenIt == controller.m_tokenToView.end())
            {
                return;
            }
            const auto viewIt = controller.m_views.find(tokenIt->second);
            if (viewIt == controller.m_views.end())
            {
                return;
            }

            handle = viewIt->second->handle;
            if (listenerName == kReadyListener)
            {
                ready = viewIt->second->onDOMReady;
            }
            else if (listenerName == kTextInputListener)
            {
                textInputBrowser = viewIt->second->browser;
            }
            else
            {
                const auto listenerIt = viewIt->second->listeners.find(listenerName);
                if (listenerIt != viewIt->second->listeners.end())
                {
                    listener = listenerIt->second;
                }
            }
        }

        if (textInputBrowser != nullptr)
        {
            if (payload == "1")
            {
                textInputBrowser->SetTextInputActive(true);
            }
            else if (payload == "0")
            {
                textInputBrowser->SetTextInputActive(false);
            }
            return;
        }

        const bool previousDispatchState = s_inDispatch;
        s_inDispatch = true;
        try
        {
            if (ready != nullptr)
            {
                ready(handle);
            }
            else if (listener != nullptr)
            {
                listener(payload.c_str());
            }
        }
        catch (const std::exception& error)
        {
            spdlog::error("{}: consumer callback failed: {}", NameOf(ViewAPIController), error.what());
        }
        catch (...)
        {
            spdlog::error("{}: consumer callback failed", NameOf(ViewAPIController));
        }
        s_inDispatch = previousDispatchState;
    }

    std::string ViewAPIController::DecodeStringArgument(const char* a_arg)
    {
        if (a_arg == nullptr)
        {
            return {};
        }
        try
        {
            const auto value = nlohmann::json::parse(a_arg);
            return value.is_string() ? value.get<std::string>() : value.dump();
        }
        catch (...)
        {
            return {};
        }
    }

    bool ViewAPIController::IsSafeName(std::string_view a_name)
    {
        if (a_name.empty() || a_name.size() > 96)
        {
            return false;
        }
        return std::all_of(a_name.begin(), a_name.end(), [](char c) {
            const auto value = static_cast<unsigned char>(c);
            return std::isalnum(value) != 0 || c == '_' || c == '-' || c == '.' || c == '$';
        });
    }

    std::string ViewAPIController::GenerateToken()
    {
        std::array<std::uint8_t, 16> bytes{};
        if (BCryptGenRandom(nullptr,
                            bytes.data(),
                            static_cast<ULONG>(bytes.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        {
            spdlog::error("{}: BCryptGenRandom failed", NameOf(ViewAPIController));
            return {};
        }

        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.resize(bytes.size() * 2);
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            result[i * 2] = hex[bytes[i] >> 4];
            result[i * 2 + 1] = hex[bytes[i] & 0x0F];
        }
        return result;
    }

    std::shared_ptr<ViewAPIController::ViewEntry> ViewAPIController::GetEntry(
        Meridian::UI::View::ViewHandle a_view) const
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return nullptr;
        }
        std::lock_guard lock(m_mutex);
        const auto it = m_views.find(a_view);
        return it == m_views.end() ? nullptr : it->second;
    }

    void ViewAPIController::ReleaseBrowser(Meridian::UI::IUIPlatformAPI::BrowserRefHandle a_handle)
    {
        if (a_handle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
        {
            return;
        }
        if (s_inDispatch)
        {
            SKSE::GetTaskInterface()->AddTask([a_handle]() {
                PublicAPIController::GetSingleton().ReleaseBrowserHandle(a_handle);
            });
            return;
        }
        PublicAPIController::GetSingleton().ReleaseBrowserHandle(a_handle);
    }
}
