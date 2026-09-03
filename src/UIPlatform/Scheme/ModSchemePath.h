#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Meridian::Scheme
{
    struct ModPath
    {
        std::string modName;       // host component, validated
        std::string relativePath;  // forward-slash relative path, validated
    };

    /// <summary>Pure layer-1 validation of a mod:// URL. The query string (if
    /// any) is stripped from the raw URL first, before decoding or
    /// validation — it is never part of the served path. What remains is
    /// percent-decoded ONCE, then rejected on: empty host, empty path, any
    /// '.'/'..' segment, absolute paths, drive letters, UNC prefixes,
    /// backslashes (normalized to '/' before checks), NUL, lingering '%',
    /// and any host or path segment ending in a trailing dot or space (Win32
    /// path normalization silently strips these, so a validated string could
    /// otherwise resolve to a different file at open time — see
    /// IsValidRelativePath's comment in the .cpp). Returns nullopt on any
    /// rejection. There is no index.html/default-document fallback: only the
    /// exact file named by the path is ever served. Layer 2 (canonical
    /// containment) happens at serve time.</summary>
    std::optional<ModPath> ParseModUrl(std::string_view a_url);

    /// <summary>MIME from the path's extension (case-insensitive); fallback
    /// application/octet-stream.</summary>
    std::string_view MimeForPath(std::string_view a_path);
}
