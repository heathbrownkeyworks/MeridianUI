#include "ModSchemeHandler.h"

#include "Scheme/ModSchemePath.h"
#include "Render/LogThrottle.h"
#include "Utils/PathUtils.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

namespace Meridian::Scheme
{
    namespace
    {
        // Shared across every handler instance, unlike FrameTransport's and
        // RenderHost's throttles, which are per-instance members confined to one
        // subsystem thread each. CEF only guarantees CefResourceHandler methods
        // are called "in sequence but not from a dedicated thread" for a SINGLE
        // handler instance -- concurrent Open() calls from different in-flight
        // requests (routine when a page references several missing assets) can
        // race on this static's non-atomic windowStart/suppressed fields. Guard
        // both the ShouldLog() check and the log emission with s_notFoundMutex so
        // the shared one-window-across-all-requests behavior stays correct.
        Meridian::Render::LogThrottle s_notFoundThrottle;
        std::mutex s_notFoundMutex;

        void LogNotFound(const std::string& a_url)
        {
            std::lock_guard lock(s_notFoundMutex);
            std::uint32_t suppressed = 0;
            if (s_notFoundThrottle.ShouldLog(suppressed))
            {
                spdlog::warn("ModSchemeHandler: 404 for \"{}\" ({} more suppressed in the last window)", a_url, suppressed);
            }
        }
    }

    bool ModSchemeHandler::Open(CefRefPtr<CefRequest> a_request, bool& a_handleRequest, CefRefPtr<CefCallback> a_callback)
    {
        // Handled synchronously below either way -- the file read is a plain
        // buffered disk read of a UI asset, not worth deferring to a callback.
        a_handleRequest = true;

        const std::string url = a_request->GetURL().ToString();
        const auto parsed = ParseModUrl(url);
        if (!parsed.has_value())
        {
            LogNotFound(url);
            return true;
        }

        const auto root = Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot()) / parsed->modName;
        const auto full = root / parsed->relativePath;

        // Layer 2: canonicalize via the OS on BOTH sides before the containment
        // check, so OS-side normalization (8.3 names, junctions, etc.) can't
        // diverge from the string ParseModUrl already validated. weakly_canonical
        // works even when the mod folder doesn't exist yet -- it canonicalizes the
        // longest existing prefix and lexically normalizes the rest.
        std::error_code ec;
        const auto canonicalRoot = std::filesystem::weakly_canonical(root, ec);
        if (ec)
        {
            LogNotFound(url);
            return true;
        }
        const auto canonicalFull = std::filesystem::weakly_canonical(full, ec);
        if (ec)
        {
            LogNotFound(url);
            return true;
        }

        const auto relative = canonicalFull.lexically_relative(canonicalRoot);
        if (relative.empty() || relative.begin()->string() == "..")
        {
            LogNotFound(url);
            return true;
        }

        std::ifstream file(canonicalFull, std::ios::binary | std::ios::ate);
        if (!file)
        {
            LogNotFound(url);
            return true;
        }

        const auto size = file.tellg();
        if (size < 0)
        {
            LogNotFound(url);
            return true;
        }

        m_buffer.resize(static_cast<std::size_t>(size));
        file.seekg(0, std::ios::beg);
        if (!m_buffer.empty() && !file.read(m_buffer.data(), static_cast<std::streamsize>(m_buffer.size())))
        {
            LogNotFound(url);
            m_buffer.clear();
            return true;
        }

        m_statusCode = 200;
        m_mimeType = std::string(MimeForPath(parsed->relativePath));
        return true;
    }

    void ModSchemeHandler::GetResponseHeaders(CefRefPtr<CefResponse> a_response, int64_t& a_responseLength, CefString& a_redirectUrl)
    {
        a_response->SetStatus(m_statusCode);
        a_response->SetMimeType(m_mimeType);
        a_responseLength = (m_statusCode == 200) ? static_cast<int64_t>(m_buffer.size()) : 0;
    }

    bool ModSchemeHandler::Read(void* a_dataOut, int a_bytesToRead, int& a_bytesRead, CefRefPtr<CefResourceReadCallback> a_callback)
    {
        const std::size_t remaining = m_buffer.size() - m_readOffset;
        if (remaining == 0 || a_bytesToRead <= 0)
        {
            a_bytesRead = 0;
            return false;
        }

        const std::size_t toCopy = std::min(remaining, static_cast<std::size_t>(a_bytesToRead));
        std::memcpy(a_dataOut, m_buffer.data() + m_readOffset, toCopy);
        m_readOffset += toCopy;
        a_bytesRead = static_cast<int>(toCopy);
        return true;
    }

    void ModSchemeHandler::Cancel()
    {
        // Nothing outstanding to unwind: Open() never defers via callback, so
        // there is no in-flight async operation for Cancel() to interrupt.
    }

    CefRefPtr<CefResourceHandler> ModSchemeHandlerFactory::Create(CefRefPtr<CefBrowser> a_browser,
                                                                   CefRefPtr<CefFrame> a_frame,
                                                                   const CefString& a_schemeName,
                                                                   CefRefPtr<CefRequest> a_request)
    {
        return new ModSchemeHandler();
    }
}
