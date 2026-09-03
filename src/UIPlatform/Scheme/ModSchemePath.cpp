#include "Scheme/ModSchemePath.h"

#include <array>
#include <cctype>
#include <utility>

namespace Meridian::Scheme
{
    namespace
    {
        // Exact-case match is safe: CEF normalizes the scheme portion of a URL to
        // lowercase before this handler ever sees it, so a case-sensitive compare
        // here cannot 404 a legitimate "MOD://..." request — there isn't one.
        constexpr std::string_view kSchemePrefix = "mod://";

        bool IsHexDigit(char a_c)
        {
            return (a_c >= '0' && a_c <= '9') || (a_c >= 'a' && a_c <= 'f') || (a_c >= 'A' && a_c <= 'F');
        }

        int HexValue(char a_c)
        {
            if (a_c >= '0' && a_c <= '9')
            {
                return a_c - '0';
            }
            if (a_c >= 'a' && a_c <= 'f')
            {
                return a_c - 'a' + 10;
            }
            return a_c - 'A' + 10;
        }

        // Percent-decodes a string exactly once. Rejects malformed escapes
        // (a trailing '%' or non-hex digits following it) and any decoded
        // NUL byte by returning nullopt.
        std::optional<std::string> PercentDecodeOnce(std::string_view a_in)
        {
            std::string out;
            out.reserve(a_in.size());

            for (std::size_t i = 0; i < a_in.size(); ++i)
            {
                const char c = a_in[i];
                if (c == '%')
                {
                    if (i + 2 >= a_in.size() || !IsHexDigit(a_in[i + 1]) || !IsHexDigit(a_in[i + 2]))
                    {
                        return std::nullopt;
                    }
                    const char decoded = static_cast<char>((HexValue(a_in[i + 1]) << 4) | HexValue(a_in[i + 2]));
                    if (decoded == '\0')
                    {
                        return std::nullopt;
                    }
                    out.push_back(decoded);
                    i += 2;
                }
                else
                {
                    out.push_back(c);
                }
            }

            return out;
        }

        void NormalizeBackslashes(std::string& a_s)
        {
            for (char& c : a_s)
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }
        }

        bool HasLingeringPercent(std::string_view a_s)
        {
            return a_s.find('%') != std::string_view::npos;
        }

        // Validates a normalized (backslashes already converted to '/'),
        // percent-decoded relative path: no empty/./..  segments, and no
        // segment containing ':' (drive letters). Also rejects any segment
        // ending in '.' or ' ' — Win32->NT path conversion silently strips
        // trailing dots/spaces from each component, so a validated string
        // could otherwise resolve to a different file at open time. The same
        // trailing-dot/space rule is applied to the host in ParseModUrl,
        // since the host is joined as a directory component too. This is
        // layer-1 validation only: by the time a path reaches here the query
        // string has already been stripped from the raw URL (ParseModUrl,
        // before decoding), and there is no index.html/default-document
        // fallback anywhere in the pipeline — only the exact file named by
        // the (fully validated) path is ever served.
        bool IsValidRelativePath(std::string_view a_path)
        {
            if (a_path.empty())
            {
                return false;
            }

            std::size_t start = 0;
            while (start <= a_path.size())
            {
                const std::size_t slash = a_path.find('/', start);
                const std::string_view segment =
                    (slash == std::string_view::npos) ? a_path.substr(start) : a_path.substr(start, slash - start);

                if (segment.empty() || segment == "." || segment == "..")
                {
                    return false;
                }
                if (segment.find(':') != std::string_view::npos)
                {
                    return false;
                }
                // Win32->NT path conversion silently strips trailing dots and spaces
                // from each path component, so "file.html." or "dir. " would resolve
                // to something other than the string we validated here. Reject at
                // this layer rather than let the mismatch surface at file-open time.
                if (segment.back() == '.' || segment.back() == ' ')
                {
                    return false;
                }

                if (slash == std::string_view::npos)
                {
                    break;
                }
                start = slash + 1;
            }

            return true;
        }
    }

    std::optional<ModPath> ParseModUrl(std::string_view a_url)
    {
        if (a_url.substr(0, kSchemePrefix.size()) != kSchemePrefix)
        {
            return std::nullopt;
        }

        const std::string_view afterScheme = a_url.substr(kSchemePrefix.size());

        // Strip the query string BEFORE any decoding/validation: it's not part of
        // the served path (spec-typical resource-handler behavior), and leaving it
        // attached would poison relativePath (guaranteed 404) and MimeForPath's
        // extension lookup (guaranteed octet-stream) for the ubiquitous
        // cache-busting "?v=123" case.
        const std::size_t queryPos = afterScheme.find('?');
        const std::string_view rest = (queryPos == std::string_view::npos) ? afterScheme : afterScheme.substr(0, queryPos);

        const std::size_t slashPos = rest.find('/');
        if (slashPos == std::string_view::npos)
        {
            // No '/' at all means there is no path component (e.g. "mod://MyMod").
            return std::nullopt;
        }

        const std::string_view rawHost = rest.substr(0, slashPos);
        const std::string_view rawPath = rest.substr(slashPos + 1);

        if (rawHost.empty())
        {
            return std::nullopt;
        }

        auto decodedHostOpt = PercentDecodeOnce(rawHost);
        auto decodedPathOpt = PercentDecodeOnce(rawPath);
        if (!decodedHostOpt.has_value() || !decodedPathOpt.has_value())
        {
            return std::nullopt;
        }

        std::string host = std::move(*decodedHostOpt);
        std::string path = std::move(*decodedPathOpt);

        NormalizeBackslashes(host);
        NormalizeBackslashes(path);

        if (HasLingeringPercent(host) || HasLingeringPercent(path))
        {
            return std::nullopt;
        }

        if (host.empty() || host.find('/') != std::string::npos)
        {
            return std::nullopt;
        }
        if (host == "." || host == ".." || host.find(':') != std::string::npos)
        {
            return std::nullopt;
        }
        // Same Win32->NT trailing dot/space stripping that IsValidRelativePath
        // guards against per path segment applies to the host too: the handler
        // joins modName as a directory component when resolving the mod root.
        if (host.back() == '.' || host.back() == ' ')
        {
            return std::nullopt;
        }

        if (host.find('\0') != std::string::npos || path.find('\0') != std::string::npos)
        {
            return std::nullopt;
        }

        if (!IsValidRelativePath(path))
        {
            return std::nullopt;
        }

        return ModPath{std::move(host), std::move(path)};
    }

    namespace
    {
        std::string_view ExtensionOf(std::string_view a_path)
        {
            const std::size_t dot = a_path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= a_path.size())
            {
                return {};
            }
            return a_path.substr(dot + 1);
        }

        std::string ToLower(std::string_view a_s)
        {
            std::string out(a_s);
            for (char& c : out)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return out;
        }
    }

    std::string_view MimeForPath(std::string_view a_path)
    {
        static const std::array<std::pair<std::string_view, std::string_view>, 17> kMimeTable{{
            {"html", "text/html"},
            {"htm", "text/html"},
            {"js", "text/javascript"},
            {"mjs", "text/javascript"},
            {"css", "text/css"},
            {"json", "application/json"},
            {"png", "image/png"},
            {"jpg", "image/jpeg"},
            {"jpeg", "image/jpeg"},
            {"gif", "image/gif"},
            {"svg", "image/svg+xml"},
            {"webp", "image/webp"},
            {"woff", "font/woff"},
            {"woff2", "font/woff2"},
            {"ttf", "font/ttf"},
            {"wasm", "application/wasm"},
            {"txt", "text/plain"},
        }};

        const std::string ext = ToLower(ExtensionOf(a_path));
        for (const auto& [key, mime] : kMimeTable)
        {
            if (ext == key)
            {
                return mime;
            }
        }

        return "application/octet-stream";
    }
}
