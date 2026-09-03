#include "Scheme/ModSchemePath.h"

#include <iostream>

namespace
{
    using namespace Meridian::Scheme;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestAccepts()
    {
        {
            const auto r = ParseModUrl("mod://MyMod/index.html");
            Expect(r.has_value(), "simple mod url accepted");
            Expect(r.has_value() && r->modName == "MyMod", "host parsed as modName");
            Expect(r.has_value() && r->relativePath == "index.html", "path parsed as relativePath");
        }
        {
            const auto r = ParseModUrl("mod://MyMod/ui/js/app.js");
            Expect(r.has_value(), "nested path accepted");
            Expect(r.has_value() && r->relativePath == "ui/js/app.js", "nested path preserved");
        }
        {
            const auto r = ParseModUrl("mod://MyMod/my%20file.png");
            Expect(r.has_value(), "percent-encoded space accepted");
            Expect(r.has_value() && r->relativePath == "my file.png", "percent-encoded space decoded");
        }
    }

    void TestRejectsTraversal()
    {
        Expect(!ParseModUrl("mod://MyMod/../Skyrim.esm").has_value(), "leading .. segment rejected");
        Expect(!ParseModUrl("mod://MyMod/./x.html").has_value(), "single-dot segment rejected");
        Expect(!ParseModUrl("mod://MyMod/ui/../../x").has_value(), "embedded .. segment rejected");
        Expect(!ParseModUrl("mod://MyMod/%2e%2e/x").has_value(), "percent-encoded .. rejected");
        Expect(!ParseModUrl("mod://MyMod/ui\\..\\x").has_value(), "backslash traversal rejected");
    }

    void TestRejectsEmptyHostOrPath()
    {
        Expect(!ParseModUrl("mod:///x.html").has_value(), "empty host rejected");
        Expect(!ParseModUrl("mod://MyMod/").has_value(), "empty path with trailing slash rejected");
        Expect(!ParseModUrl("mod://MyMod").has_value(), "empty path with no slash rejected");
    }

    void TestRejectsAbsoluteAndUnc()
    {
        Expect(!ParseModUrl("mod://MyMod/C:/Windows/x").has_value(), "drive letter rejected");
        Expect(!ParseModUrl("mod://MyMod//server/share").has_value(), "forward-slash UNC rejected");
        Expect(!ParseModUrl("mod://MyMod/\\\\server\\share").has_value(), "backslash UNC rejected");
    }

    void TestRejectsEncodingTricks()
    {
        Expect(!ParseModUrl("mod://MyMod/a%2fb%252f").has_value(), "lingering percent after single decode rejected");
        Expect(!ParseModUrl("mod://MyMod/%00hidden").has_value(), "embedded NUL via percent-encoding rejected");
        Expect(!ParseModUrl("mod://MyMod/%252e%252e/x").has_value(), "double-encoded .. leaves a lingering percent and is rejected");

        // Self-review additions: a literal (non-percent-encoded) NUL byte anywhere
        // in the URL, and a host component that is itself a traversal segment.
        const std::string nulInPath = std::string("mod://MyMod/x") + '\0' + "y.html";
        Expect(!ParseModUrl(nulInPath).has_value(), "literal embedded NUL byte in path rejected (not just percent-encoded)");

        Expect(!ParseModUrl("mod://../x.html").has_value(), "host of '..' rejected (host is joined as a directory segment)");
        Expect(!ParseModUrl("mod://./x.html").has_value(), "host of '.' rejected");
        Expect(!ParseModUrl("mod://%2e%2e/x.html").has_value(), "percent-encoded '..' host rejected");
        Expect(!ParseModUrl("mod://C:/x.html").has_value(), "host containing ':' rejected");
    }

    void TestRejectsWrongScheme()
    {
        Expect(!ParseModUrl("notmod://x/y").has_value(), "wrong scheme rejected");
        Expect(!ParseModUrl("MOD://MyMod/x.html").has_value(),
               "uppercase scheme rejected (CEF lowercases the scheme before this handler runs, so this cannot 404 a real request)");
    }

    // Finding 1: query strings are not part of the served path and must be
    // stripped before decoding/validation, or a routine cache-busting URL like
    // "?v=123" would poison relativePath (guaranteed 404) and MimeForPath's
    // extension lookup (guaranteed octet-stream).
    void TestQueryStrings()
    {
        {
            const auto r = ParseModUrl("mod://MyMod/index.html?v=123");
            Expect(r.has_value(), "query string is stripped, not treated as part of the path");
            Expect(r.has_value() && r->relativePath == "index.html", "relativePath excludes the query string");
        }
        Expect(!ParseModUrl("mod://MyMod/?x").has_value(), "query string strip leaves an empty path, which is rejected");
    }

    // Finding 2: Win32->NT path conversion silently strips trailing dots/spaces
    // per path component, so a string that validates here could resolve
    // differently than expected at file-open time. Reject trailing '.'/' ' on
    // every segment.
    void TestRejectsTrailingDotOrSpaceSegments()
    {
        Expect(!ParseModUrl("mod://MyMod/file.html.").has_value(), "trailing dot on the final segment rejected");
        Expect(!ParseModUrl("mod://MyMod/x.%20/y").has_value(), "trailing encoded space on an interior segment rejected");
        {
            const auto r = ParseModUrl("mod://MyMod/file.name.html");
            Expect(r.has_value(), "legitimate multi-dot filename still accepted");
            Expect(r.has_value() && r->relativePath == "file.name.html", "multi-dot filename preserved exactly");
        }

        // The host is also used as a directory component, so it needs the same
        // trailing dot and space rejection as path segments. Otherwise
        // "MyMod." could resolve to a different directory at file-open time.
        Expect(!ParseModUrl("mod://MyMod./x.html").has_value(), "trailing dot on host rejected");
        Expect(!ParseModUrl("mod://MyMod%20/x.html").has_value(), "trailing encoded space on host rejected");
    }

    // Pin the validation order: percent-decode once, normalize backslashes,
    // then reject a lingering '%' before handling mixed encoded traversal.
    void TestRejectsMixedSeparatorEncodedTraversal()
    {
        Expect(!ParseModUrl("mod://MyMod/..%5c..%5cx").has_value(), "encoded-backslash traversal (..%5c..%5cx) rejected");
        Expect(!ParseModUrl("mod://MyMod/ui\\..%2fx").has_value(), "literal-backslash + encoded-forward-slash traversal rejected");
    }

    void TestMimeForPath()
    {
        Expect(MimeForPath("index.html") == "text/html", "html mime");
        Expect(MimeForPath("index.htm") == "text/html", "htm mime");
        Expect(MimeForPath("app.JS") == "text/javascript", "js mime is case-insensitive");
        Expect(MimeForPath("app.mjs") == "text/javascript", "mjs mime");
        Expect(MimeForPath("style.css") == "text/css", "css mime");
        Expect(MimeForPath("data.json") == "application/json", "json mime");
        Expect(MimeForPath("img.png") == "image/png", "png mime");
        Expect(MimeForPath("img.jpg") == "image/jpeg", "jpg mime");
        Expect(MimeForPath("img.jpeg") == "image/jpeg", "jpeg mime");
        Expect(MimeForPath("img.gif") == "image/gif", "gif mime");
        Expect(MimeForPath("icon.svg") == "image/svg+xml", "svg mime");
        Expect(MimeForPath("img.webp") == "image/webp", "webp mime");
        Expect(MimeForPath("font.woff") == "font/woff", "woff mime");
        Expect(MimeForPath("font.woff2") == "font/woff2", "woff2 mime");
        Expect(MimeForPath("font.ttf") == "font/ttf", "ttf mime");
        Expect(MimeForPath("mod.wasm") == "application/wasm", "wasm mime");
        Expect(MimeForPath("readme.txt") == "text/plain", "txt mime");
        Expect(MimeForPath("file.xyz") == "application/octet-stream", "unknown extension falls back");
        Expect(MimeForPath("noextension") == "application/octet-stream", "extensionless falls back");
    }
}

int main()
{
    TestAccepts();
    TestRejectsTraversal();
    TestRejectsEmptyHostOrPath();
    TestRejectsAbsoluteAndUnc();
    TestRejectsEncodingTricks();
    TestRejectsWrongScheme();
    TestQueryStrings();
    TestRejectsTrailingDotOrSpaceSegments();
    TestRejectsMixedSeparatorEncodedTraversal();
    TestMimeForPath();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " ModSchemePath test(s) failed\n";
        return 1;
    }

    std::cout << "All ModSchemePath tests passed\n";
    return 0;
}
