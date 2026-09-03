#pragma once

// Included from BOTH processes: the browser-process CefApp (to install the
// actual handler/factory) and the subprocess CefApp (which only needs
// RegisterModScheme() so its OnRegisterCustomSchemes() can mirror the exact
// same option flags -- CEF requires the registered scheme to match across
// every process). Keep this header's includes to CEF + plain std (no
// ModSchemePath.h / LogThrottle.h -- those are pure too, but the actual
// parsing/logging lives in the .cpp, which only the browser-process target
// compiles) so it stays safe to include from the subprocess target, which
// does not see src/UIPlatform/PCH.h.
#include <include/cef_scheme.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Meridian::Scheme
{
    /// <summary>Registers the "mod" custom scheme with the option flags needed
    /// for the CEF UI to fetch()/XHR mounted mod assets: standard URL parsing,
    /// secure-context treatment, CORS, and fetch access. Must be called from
    /// both processes' CefApp::OnRegisterCustomSchemes() with IDENTICAL flags --
    /// CEF requires the registered scheme to agree across every process.</summary>
    inline void RegisterModScheme(CefRawPtr<CefSchemeRegistrar> a_registrar)
    {
        a_registrar->AddCustomScheme("mod",
                                     CEF_SCHEME_OPTION_STANDARD |
                                         CEF_SCHEME_OPTION_SECURE |
                                         CEF_SCHEME_OPTION_CORS_ENABLED |
                                         CEF_SCHEME_OPTION_FETCH_ENABLED);
    }

    /// <summary>Serves a single mod:// request from
    /// Data/MeridianUI/&lt;modName&gt;/&lt;relativePath&gt;. One instance per
    /// request (see ModSchemeHandlerFactory::Create) -- the whole file is read
    /// into m_buffer during Open() since UI assets are MBs at most, and Read()
    /// just streams out of that buffer.</summary>
    class ModSchemeHandler : public CefResourceHandler
    {
    public:
        // CefResourceHandler
        bool Open(CefRefPtr<CefRequest> a_request, bool& a_handleRequest, CefRefPtr<CefCallback> a_callback) override;
        void GetResponseHeaders(CefRefPtr<CefResponse> a_response, int64_t& a_responseLength, CefString& a_redirectUrl) override;
        bool Read(void* a_dataOut, int a_bytesToRead, int& a_bytesRead, CefRefPtr<CefResourceReadCallback> a_callback) override;
        void Cancel() override;

    private:
        std::vector<char> m_buffer;
        std::size_t m_readOffset = 0;
        int m_statusCode = 404;
        std::string m_mimeType = "text/plain";

        IMPLEMENT_REFCOUNTING(ModSchemeHandler);
    };

    /// <summary>Vends a fresh ModSchemeHandler per request. Create() is always
    /// called on the IO thread per CEF's CefSchemeHandlerFactory contract.</summary>
    class ModSchemeHandlerFactory : public CefSchemeHandlerFactory
    {
    public:
        CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> a_browser,
                                             CefRefPtr<CefFrame> a_frame,
                                             const CefString& a_schemeName,
                                             CefRefPtr<CefRequest> a_request) override;

    private:
        IMPLEMENT_REFCOUNTING(ModSchemeHandlerFactory);
    };
}
