#pragma once

#include "PCH.h"

namespace Meridian::UI::TestCase
{
    class LocalTestPage
    {
        static inline std::mutex s_thisVectorMutex;
        static inline std::vector<LocalTestPage*> s_thisVector;

        static void StaticShutdown()
        {
            std::lock_guard locker(s_thisVectorMutex);
            for (auto& ptr : s_thisVector)
            {
                ptr->Shutdown();
            }
            s_thisVector.clear();
        }

        Meridian::CEF::IBrowser* m_browser = nullptr;
        Meridian::UI::IUIPlatformAPI::BrowserRefHandle m_browserHandle = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        Meridian::CEF::IBrowser* m_browser2 = nullptr;
        Meridian::UI::IUIPlatformAPI::BrowserRefHandle m_browserHandle2 = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        Meridian::UI::IUIPlatformAPI* m_api = nullptr;

        std::vector<JS::JSFuncInfo*> m_funcInfoVector;

        std::jthread m_printThread;
        std::atomic_bool m_shutdownRequested{false};

        void RegisterPromiseNatives();

    public:
        LocalTestPage();
        ~LocalTestPage();

        void Start(Meridian::UI::IUIPlatformAPI* a_api);
        void Shutdown();
    };
}
