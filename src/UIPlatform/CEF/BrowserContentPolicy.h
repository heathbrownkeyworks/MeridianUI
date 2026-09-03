#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Meridian::CEF
{
    class BrowserContentPolicy
    {
    public:
        explicit BrowserContentPolicy(bool a_allowRemoteContent = false)
            : m_allowRemoteContent(a_allowRemoteContent)
        {
        }

        void SetAllowRemoteContent(bool a_allow);
        void EnableNativeBindings(std::string_view a_currentUrl = {});
        bool NativeBindingsEnabled() const;

        bool AllowNavigation(std::string_view a_url);
        bool AllowResource(std::string_view a_url) const;
        bool IsTrusted(std::string_view a_url) const;
        std::optional<std::string> TrustedModHost() const;

    private:
        static bool HostEquals(std::string_view a_lhs, std::string_view a_rhs);

        bool m_nativeBindingsEnabled = false;
        bool m_allowRemoteContent = false;
        std::optional<std::string> m_trustedModHost;
    };
}
