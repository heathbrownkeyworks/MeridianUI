#pragma once

#include "PCH.h"
#include "Common/Singleton.h"
#include "MeridianUIAPI/ViewAPI.h"

namespace Meridian::CEF
{
    class DefaultBrowser;
}

namespace Meridian::Controllers
{
    class ViewAPIController final : public Meridian::UI::View::IViewAPI,
                                    public Meridian::Common::Singleton<ViewAPIController>
    {
        friend class Meridian::Common::Singleton<ViewAPIController>;

        struct ViewEntry
        {
            Meridian::UI::View::ViewHandle handle = Meridian::UI::View::INVALID_VIEW_HANDLE;
            Meridian::UI::IUIPlatformAPI::BrowserRefHandle browserHandle =
                Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            std::shared_ptr<Meridian::CEF::DefaultBrowser> browser;
            std::string browserName;
            std::string token;
            Meridian::UI::View::DOMReadyCallback onDOMReady = nullptr;
            std::unordered_map<std::string, Meridian::UI::View::ListenerCallback> listeners;
        };

    public:
        Meridian::UI::View::ViewHandle __cdecl CreateView(
            const Meridian::UI::View::ViewCreateInfo* a_info) override;
        void __cdecl DestroyView(Meridian::UI::View::ViewHandle a_view) override;
        bool __cdecl IsValid(Meridian::UI::View::ViewHandle a_view) const override;
        bool __cdecl IsReady(Meridian::UI::View::ViewHandle a_view) const override;
        bool __cdecl RegisterListener(Meridian::UI::View::ViewHandle a_view,
                                      const char* a_name,
                                      Meridian::UI::View::ListenerCallback a_callback) override;
        bool __cdecl ExecuteJavaScript(Meridian::UI::View::ViewHandle a_view,
                                       const char* a_script) override;
        bool __cdecl Show(Meridian::UI::View::ViewHandle a_view) override;
        bool __cdecl Hide(Meridian::UI::View::ViewHandle a_view) override;
        Meridian::UI::View::FocusResult __cdecl TryFocus(
            Meridian::UI::View::ViewHandle a_view,
            Meridian::UI::View::FocusMode a_mode) override;
        void __cdecl Unfocus(Meridian::UI::View::ViewHandle a_view) override;
        bool __cdecl HasFocus(Meridian::UI::View::ViewHandle a_view) const override;
        bool __cdecl HasAnyFocus() const override;

        void BeginShutdown();
        bool IsShuttingDown() const;

    private:
        ViewAPIController() = default;

        static void Dispatch(const char** a_args, int a_argsCount);
        static std::string DecodeStringArgument(const char* a_arg);
        static bool IsSafeName(std::string_view a_name);
        static std::string GenerateToken();

        std::shared_ptr<ViewEntry> GetEntry(Meridian::UI::View::ViewHandle a_view) const;
        void ReleaseBrowser(Meridian::UI::IUIPlatformAPI::BrowserRefHandle a_handle);

        mutable std::mutex m_mutex;
        std::unordered_map<Meridian::UI::View::ViewHandle, std::shared_ptr<ViewEntry>> m_views;
        std::unordered_map<std::string, Meridian::UI::View::ViewHandle> m_tokenToView;
        std::unordered_set<std::string> m_ownedBrowserNames;
        std::atomic<Meridian::UI::View::ViewHandle> m_nextHandle{1};
        std::atomic_bool m_isShuttingDown{false};
        static inline thread_local bool s_inDispatch = false;
    };
}
