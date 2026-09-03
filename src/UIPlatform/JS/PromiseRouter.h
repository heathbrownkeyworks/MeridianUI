#pragma once

#include "PCH.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Meridian::JS
{
    /// <summary>
    /// Routes resolver settles back to the browser that issued the call.
    /// callIds are allocated per-renderer-process (each renderer's
    /// PromiseRegistry starts its own counter at 1), so callId alone is NOT
    /// unique process-wide — two browsers can concurrently hold in-flight
    /// calls with the same callId. Routes are therefore keyed on the
    /// composite (browserId, callId) pair; the resolver holds that same
    /// pair, and the router holds the browser ref. A browser's routes are
    /// dropped at its shutdown, which is what makes a post-close settle a
    /// safe no-op instead of a use-after-free.
    ///
    /// The router also RETAINS every resolver it ever creates (see
    /// RetainResolver / m_resolvers below) for the process lifetime, instead
    /// of the resolver freeing itself on settle. This is a deliberate
    /// memory/safety tradeoff: the resolver's contract is "callable from any
    /// thread, at any time, including after the browser has closed," which
    /// an author-held pointer can outlive by an unbounded amount — there is
    /// no observable point at which the platform can prove no thread still
    /// holds the pointer, so any earlier reclamation (including the
    /// previous `delete this` on settle) can race a late or duplicate
    /// settle into a use-after-free. Retaining for process lifetime trades
    /// ~48 bytes per call (the resolver object itself: vtable pointer +
    /// int browserId + int32 callId + atomic_bool settled; unique_ptr adds
    /// no extra allocation of its own) for an uncrashable contract.
    /// Reclaimed only at process end.
    /// </summary>
    class PromiseRouter
    {
    public:
        static PromiseRouter& GetSingleton();

        /// <summary>Registers the route for (a_browserId, a_callId). A null
        /// a_browser is a caller bug — nothing could ever settle back to it —
        /// so it is logged and dropped rather than routed.</summary>
        void AddRoute(int a_browserId, std::int32_t a_callId, CefRefPtr<CefBrowser> a_browser);
        /// <summary>Removes and returns the route; nullptr when already
        /// dropped (browser closed) or already settled.</summary>
        CefRefPtr<CefBrowser> TakeRoute(int a_browserId, std::int32_t a_callId);
        /// <summary>Drops every route for the browser (identified by CEF id).
        /// Called from DefaultBrowser::BeginShutdown.</summary>
        void DropBrowserRoutes(int a_browserId);

        /// <summary>Takes ownership of a_resolver into the append-only
        /// retention store and returns a non-owning pointer to it. The
        /// router never erases entries — see the class comment for why.</summary>
        Meridian::JS::IJSPromiseResolver* RetainResolver(std::unique_ptr<Meridian::JS::IJSPromiseResolver> a_resolver);

    protected:
        std::mutex m_mutex;
        std::unordered_map<std::uint64_t, CefRefPtr<CefBrowser>> m_routes;

        std::mutex m_resolversMutex;
        std::vector<std::unique_ptr<Meridian::JS::IJSPromiseResolver>> m_resolvers;
    };

    /// <summary>Allocates the one-shot IJSPromiseResolver for
    /// (a_browserId, a_callId) (see PromiseResolver in PromiseRouter.cpp) and
    /// hands it to PromiseRouter::RetainResolver for process-lifetime
    /// retention. Caller passes the returned (non-owning) pointer to the
    /// promise callback.</summary>
    Meridian::JS::IJSPromiseResolver* CreatePromiseResolver(int a_browserId, std::int32_t a_callId);
}
