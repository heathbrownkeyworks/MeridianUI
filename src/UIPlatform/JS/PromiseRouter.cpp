#include "PromiseRouter.h"
#include "PromiseRouteKey.h"
#include "IPC.h"

namespace Meridian::JS
{
    PromiseRouter& PromiseRouter::GetSingleton()
    {
        static PromiseRouter singleton;
        return singleton;
    }

    void PromiseRouter::AddRoute(int a_browserId, std::int32_t a_callId, CefRefPtr<CefBrowser> a_browser)
    {
        if (a_browser == nullptr)
        {
            spdlog::warn("PromiseRouter::AddRoute: dropping route for call {} (browser {}) — browser is null", a_callId, a_browserId);
            return;
        }

        std::lock_guard lock(m_mutex);
        m_routes[MakeRouteKey(a_browserId, a_callId)] = std::move(a_browser);
    }

    CefRefPtr<CefBrowser> PromiseRouter::TakeRoute(int a_browserId, std::int32_t a_callId)
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_routes.find(MakeRouteKey(a_browserId, a_callId));
        if (it == m_routes.end())
        {
            return nullptr;
        }

        auto browser = it->second;
        m_routes.erase(it);
        return browser;
    }

    void PromiseRouter::DropBrowserRoutes(int a_browserId)
    {
        std::lock_guard lock(m_mutex);
        const auto browserIdBits = static_cast<std::uint32_t>(a_browserId);
        for (auto it = m_routes.begin(); it != m_routes.end();)
        {
            if (RouteKeyBrowserIdBits(it->first) == browserIdBits)
            {
                it = m_routes.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    Meridian::JS::IJSPromiseResolver* PromiseRouter::RetainResolver(std::unique_ptr<Meridian::JS::IJSPromiseResolver> a_resolver)
    {
        std::lock_guard lock(m_resolversMutex);
        m_resolvers.push_back(std::move(a_resolver));
        return m_resolvers.back().get();
    }

    /// <summary>
    /// One-shot settle handle for a single promise call. Allocated with
    /// `new` per inbound IPC_JS_PROMISE_CALL and immediately handed to
    /// PromiseRouter::RetainResolver, which owns it for the process
    /// lifetime — Settle NEVER deletes `this` (see PromiseRouter's class
    /// comment for why: the "callable from any thread, at any time" contract
    /// admits no safe earlier reclamation point, since an author may hold
    /// the pointer past browser close for an unbounded time). The atomic
    /// `m_settled` guard still enforces one-shot semantics — the first
    /// Resolve/Reject wins and sends the result; every later call, from any
    /// thread, is now unconditionally a safe logged no-op, since the object
    /// itself is never freed.
    ///
    /// Stores both m_browserId and m_callId because the route it settles is
    /// keyed on the composite pair (see PromiseRouter's class comment on why
    /// callId alone is not process-wide unique). The wire message sent below
    /// still carries only the plain int32 callId — the composite key exists
    /// solely inside this platform-side router; the renderer's
    /// PromiseRegistry is per-process, so the bare callId is already unique
    /// there and needs no browserId to disambiguate.
    /// </summary>
    class PromiseResolver final : public Meridian::JS::IJSPromiseResolver
    {
    public:
        PromiseResolver(int a_browserId, std::int32_t a_callId) : m_browserId(a_browserId), m_callId(a_callId) {}

        void __cdecl Resolve(const char* a_jsonPayload) override { Settle(true, a_jsonPayload == nullptr ? "null" : a_jsonPayload); }
        void __cdecl Reject(const char* a_errorMessage) override { Settle(false, a_errorMessage == nullptr ? "" : a_errorMessage); }

    private:
        void Settle(bool a_ok, const std::string& a_payload)
        {
            const auto browserId = m_browserId;
            const auto callId = m_callId;

            if (m_settled.exchange(true, std::memory_order_acq_rel))
            {
                spdlog::debug("PromiseResolver: duplicate settle for call {} (browser {}) ignored", callId, browserId);
                return;
            }
            const auto browser = PromiseRouter::GetSingleton().TakeRoute(browserId, callId);
            if (browser == nullptr)
            {
                spdlog::debug("PromiseResolver: route for call {} (browser {}) already gone (browser closed)", callId, browserId);
                return;
            }
            const auto frame = browser->GetMainFrame();
            if (frame == nullptr)
            {
                return;
            }
            auto message = CefProcessMessage::Create(IPC_JS_PROMISE_RESULT);
            auto args = message->GetArgumentList();
            // Wire callId stays plain int32 — see class comment above.
            args->SetInt(0, callId);
            args->SetBool(1, a_ok);
            args->SetString(2, a_payload);
            frame->SendProcessMessage(CefProcessId::PID_RENDERER, message);
        }

        const int m_browserId;
        const std::int32_t m_callId;
        std::atomic_bool m_settled{false};
    };

    Meridian::JS::IJSPromiseResolver* CreatePromiseResolver(int a_browserId, std::int32_t a_callId)
    {
        return PromiseRouter::GetSingleton().RetainResolver(std::make_unique<PromiseResolver>(a_browserId, a_callId));
    }
}
