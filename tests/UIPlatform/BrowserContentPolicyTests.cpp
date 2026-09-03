#include "CEF/BrowserContentPolicy.h"

#include <iostream>

namespace
{
    int g_failures = 0;
    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    Meridian::CEF::BrowserContentPolicy releaseDefault;
    Expect(!releaseDefault.AllowNavigation("https://example.com/"), "release default rejects remote navigation");
    Expect(!releaseDefault.AllowResource("https://cdn.example.com/app.js"), "release default rejects remote resources");
    Expect(releaseDefault.AllowNavigation("mod://LocalOnly/index.html"), "release default accepts local mod content");
    Expect(!releaseDefault.AllowNavigation("mod://OtherMod/index.html"), "release default pins the first local mod host");

    Meridian::CEF::BrowserContentPolicy development(true);
    Expect(development.AllowNavigation("https://example.com/"), "explicit development opt-in allows remote navigation");
    Expect(development.AllowResource("https://cdn.example.com/app.js"), "explicit development opt-in allows remote resources");
    Expect(!development.AllowNavigation("file:///C:/secret.txt"), "development opt-in does not enable file navigation");
    Expect(!development.AllowResource("file:///C:/secret.txt"), "development opt-in does not enable file resources");
    Expect(!development.AllowNavigation("ftp://example.com/file"), "development opt-in is limited to HTTP(S)");

    Meridian::CEF::BrowserContentPolicy native(true);
    native.EnableNativeBindings();
    Expect(!native.AllowNavigation("https://example.com/"), "native browser rejects remote initial URL");
    Expect(!native.AllowNavigation("file:///C:/ui/index.html"), "native browser rejects file URLs");
    Expect(native.AllowNavigation("mod://MyMod/index.html"), "native browser accepts a valid mod URL");
    Expect(native.TrustedModHost() == "MyMod", "first mod navigation pins the trusted host");
    Expect(native.AllowNavigation("mod://mymod/pages/settings.html"), "trusted host comparison is case-insensitive");
    Expect(!native.AllowNavigation("mod://OtherMod/index.html"), "native browser rejects cross-mod navigation");
    Expect(!native.AllowNavigation("https://example.com/redirect"), "native browser rejects remote redirect targets");
    Expect(native.AllowResource("mod://MyMod/app.js"), "trusted mod resource accepted");
    Expect(!native.AllowResource("mod://OtherMod/app.js"), "cross-mod subresource rejected");
    Expect(!native.AllowResource("https://cdn.example.com/app.js"), "remote script resource rejected");
    Expect(native.AllowResource("data:image/png;base64,AA=="), "data resource accepted in trusted document");
    Expect(native.AllowResource("blob:opaque-id"), "blob resource accepted in trusted document");
    Expect(native.IsTrusted("mod://MyMod/index.html"), "pinned mod document can expose bindings");
    Expect(!native.IsTrusted("https://example.com/"), "remote document cannot expose bindings");

    Meridian::CEF::BrowserContentPolicy lateLocal;
    lateLocal.EnableNativeBindings("mod://LateMod/index.html");
    Expect(lateLocal.IsTrusted("mod://LateMod/index.html"), "late binding adopts an already-local mod document");

    Meridian::CEF::BrowserContentPolicy lateRemote;
    lateRemote.EnableNativeBindings("https://example.com/");
    Expect(!lateRemote.IsTrusted("https://example.com/"), "late binding never adopts a remote document");
    Expect(lateRemote.AllowNavigation("mod://SafeAfterRemote/index.html"), "late-bound remote browser may recover by navigating to mod content");
    Expect(lateRemote.IsTrusted("mod://SafeAfterRemote/index.html"), "recovered mod document can expose bindings");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " BrowserContentPolicy test(s) failed\n";
        return 1;
    }
    std::cout << "All BrowserContentPolicy tests passed\n";
    return 0;
}
