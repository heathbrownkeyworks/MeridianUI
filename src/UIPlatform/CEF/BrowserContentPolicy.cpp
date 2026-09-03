#include "CEF/BrowserContentPolicy.h"

#include "Scheme/ModSchemePath.h"

#include <algorithm>
#include <cctype>

namespace
{
    bool StartsWithCaseInsensitive(std::string_view a_value, std::string_view a_prefix)
    {
        return a_value.size() >= a_prefix.size() &&
               std::equal(a_prefix.begin(), a_prefix.end(), a_value.begin(), [](unsigned char a_left, unsigned char a_right) {
                   return std::tolower(a_left) == std::tolower(a_right);
               });
    }

    bool IsHttpUrl(std::string_view a_url)
    {
        return StartsWithCaseInsensitive(a_url, "http://") ||
               StartsWithCaseInsensitive(a_url, "https://");
    }

    bool IsDocumentLocalResource(std::string_view a_url)
    {
        return StartsWithCaseInsensitive(a_url, "data:") ||
               StartsWithCaseInsensitive(a_url, "blob:") ||
               a_url == "about:blank";
    }
}

namespace Meridian::CEF
{
    void BrowserContentPolicy::SetAllowRemoteContent(bool a_allow)
    {
        m_allowRemoteContent = a_allow;
    }

    void BrowserContentPolicy::EnableNativeBindings(std::string_view a_currentUrl)
    {
        m_nativeBindingsEnabled = true;
        if (!m_trustedModHost && !a_currentUrl.empty())
        {
            if (const auto parsed = Meridian::Scheme::ParseModUrl(a_currentUrl))
            {
                m_trustedModHost = parsed->modName;
            }
        }
    }

    bool BrowserContentPolicy::NativeBindingsEnabled() const
    {
        return m_nativeBindingsEnabled;
    }

    bool BrowserContentPolicy::HostEquals(std::string_view a_lhs, std::string_view a_rhs)
    {
        return a_lhs.size() == a_rhs.size() &&
               std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](unsigned char a_left, unsigned char a_right) {
                   return std::tolower(a_left) == std::tolower(a_right);
               });
    }

    bool BrowserContentPolicy::AllowNavigation(std::string_view a_url)
    {
        if (!m_nativeBindingsEnabled && m_allowRemoteContent && IsHttpUrl(a_url))
        {
            return true;
        }

        const auto parsed = Meridian::Scheme::ParseModUrl(a_url);
        if (!parsed)
        {
            return false;
        }

        if (!m_trustedModHost)
        {
            m_trustedModHost = parsed->modName;
            return true;
        }

        return HostEquals(*m_trustedModHost, parsed->modName);
    }

    bool BrowserContentPolicy::AllowResource(std::string_view a_url) const
    {
        if (!m_nativeBindingsEnabled && m_allowRemoteContent && IsHttpUrl(a_url))
        {
            return true;
        }

        if (IsDocumentLocalResource(a_url))
        {
            return true;
        }

        const auto parsed = Meridian::Scheme::ParseModUrl(a_url);
        return parsed.has_value() && m_trustedModHost.has_value() && HostEquals(*m_trustedModHost, parsed->modName);
    }

    bool BrowserContentPolicy::IsTrusted(std::string_view a_url) const
    {
        if (!m_nativeBindingsEnabled)
        {
            return true;
        }

        const auto parsed = Meridian::Scheme::ParseModUrl(a_url);
        return parsed.has_value() && m_trustedModHost.has_value() && HostEquals(*m_trustedModHost, parsed->modName);
    }

    std::optional<std::string> BrowserContentPolicy::TrustedModHost() const
    {
        return m_trustedModHost;
    }
}
