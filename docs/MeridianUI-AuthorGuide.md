# MeridianUI 1.1 — Author Guide

MeridianUI is an SKSE plugin platform that embeds Chromium (CEF) browsers in Skyrim
Special Edition and composites them over the game's frame. Its consumers are other
mod authors' SKSE plugins: you acquire an `IUIPlatformAPI*`, create named browsers,
point them at HTML, and bind C++ functions into the page's JavaScript.

This guide documents **API version 1.0** (`Meridian::UI::APIVersion` 1.0, library
`Meridian::UI::LibVersion` 1.1). Everything below is taken from the frozen headers in
`MeridianUIAPI/` and the shipped platform sources; where a rule has a one-line
statement in a header, that statement is quoted rather than paraphrased.

1. [Getting started](#1-getting-started)
2. [Browser lifecycle](#2-browser-lifecycle)
3. [JS bindings](#3-js-bindings)
4. [The `mod://` scheme](#4-the-mod-scheme)
5. [Settings and INI](#5-settings-and-ini)
6. [Focus, input, toggle keys](#6-focus-input-toggle-keys)
7. [Coexistence and limitations](#7-coexistence-and-limitations)

---

## 1. Getting started

### What you ship

One ordinary SKSE plugin: `Data/SKSE/Plugins/<YourPlugin>.dll`, compiled against the
`MeridianUIAPI` headers. Do not redistribute MeridianUI itself — declare it as a
required dependency of your mod and let users install it.

Copy the whole `MeridianUIAPI` folder into your project:

| Header | What you need it for |
| --- | --- |
| `API.h` | `IUIPlatformAPI`, `APIMessageType`, the request/response message structs |
| `IBrowser.h` | the per-browser interface |
| `JSTypes.h` | `JSFuncInfo`, `JSPromiseFuncInfo`, `IJSPromiseResolver` |
| `Settings.h` | `Settings`, `BrowserSettings`, `RendererType` |
| `Version.h` | `LibVersion` / `APIVersion` constants and helpers |
| `SKSELoader.h` | the recommended acquisition flow (SKSE messaging) |
| `DllLoader.h` | alternative acquisition via MeridianUI's DLL exports |
| `SettingsIngest.h` | platform-side struct ingestion; consumers do not call it |

`SKSELoader.h` includes `<SKSE/SKSE.h>` and logs through spdlog, so CommonLibSSE must
be on your include path.

**Compile with MSVC `/utf-8`.** The 1.0 headers ship without a UTF-8 BOM and contain
UTF-8 punctuation in their doc comments. Without `/utf-8`, MSVC decodes them using the
system codepage and emits C4819 for every such comment. `/utf-8` is standard practice
for CommonLibSSE projects; if your CMake preset does not already set it, add it.

### Acquiring the API

Use the `SKSELoader` flow. It performs the version handshake for you and hands you the
API pointer through a callback. This example is the consumer form of the loader path
the platform's own test plugin uses (`Init3rdMethodToGetAPI` in
`src/UIPlatformTest/main.cpp`):

```cpp
#include "MeridianUIAPI/API.h"
#include "MeridianUIAPI/SKSELoader.h"

namespace MyMod
{
    void OnMeridianReady(Meridian::UI::IUIPlatformAPI* a_api);  // your code

    void InitMeridianUI()
    {
        SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
            // SKSELoader::ProcessSKSEMessage only reads `settings` for the
            // kInputLoaded/RequestAPI dispatch, but a static keeps the pointer
            // valid for that call regardless of message type. Fill it in before
            // kInputLoaded if you want non-default settings; leaving it default
            // is the normal case (see section 5).
            static Meridian::UI::Settings s_settings{};
            Meridian::UI::SKSELoader::ProcessSKSEMessage(a_msg, &s_settings);
        });

        Meridian::UI::SKSELoader::GetUIPlatformAPIWithVersionCheck([](Meridian::UI::IUIPlatformAPI* a_api) {
            OnMeridianReady(a_api);
        });
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (a_skse->IsEditor())
    {
        return false;
    }

    SKSE::Init(a_skse);
    MyMod::InitMeridianUI();
    return true;
}
```

Two listeners are in play, and both are required:

* the unnamed `RegisterListener` above receives SKSE's own lifecycle messages
  (`kPostPostLoad`, `kInputLoaded`) and forwards them, with your `Settings`, to
  `ProcessSKSEMessage`;
* `GetUIPlatformAPIWithVersionCheck` registers the second listener for the
  `"MeridianUI"` sender (`Meridian::UI::LibVersion::PROJECT_NAME`) and stores your
  ready-callback.

The `Settings` object must be alive for the duration of the `ProcessSKSEMessage` call
that dispatches it; `static` is the simplest way to be sure.

### The version handshake

`SKSELoader::ProcessSKSEMessage` implements this sequence:

1. **`kPostPostLoad`** — all plugins are loaded, so the loader dispatches
   `APIMessageType::RequestVersion` to `"MeridianUI"` with no data.
2. **`ResponseVersion`** — MeridianUI answers with a `ResponseVersionMessage`
   (`libVersion`, `apiVersion`). The loader accepts only if the installed API has the
   **same major** version as your headers and a **minor no older than** yours:

   ```cpp
   if (majorAPIVersion != Meridian::UI::APIVersion::MAJOR || minorAPIVersion < Meridian::UI::APIVersion::MINOR)
   ```

   On mismatch it logs `MeridianUI loader: can't use this API version...`, clears
   `LoaderData::s_canUseAPI`, and never requests the API. Your ready-callback is simply
   never invoked — handle that as "MeridianUI unavailable", not as an error state you
   can retry.
3. **`kInputLoaded`** — if the version check passed, the loader dispatches
   `APIMessageType::RequestAPI` carrying your `Settings`. The **first** `RequestAPI`
   from any consumer initializes the library.
4. **`ResponseAPI`** — carries `ResponseAPIMessage::API`. The loader null-checks it and
   invokes your callback with the `IUIPlatformAPI*`.

If you dispatch these messages by hand instead of using `SKSELoader`, note that the
platform validates the `RequestAPI` payload length against
`sizeof(Meridian::UI::RequestAPIMessage)` and rejects anything else.

`DllLoader.h` offers a second route (`CreateOrGetUIPlatformAPIWithVersionCheck`) that
resolves MeridianUI's DLL exports directly. It works, but it throws on a missing DLL
and gives you no version-negotiation logging; prefer `SKSELoader`.

### ABI rule: rebuild against the 1.0 headers

`Settings` and `BrowserSettings` are size-versioned. Both begin with a `structSize`
field defaulted to `sizeof(...)`, and the header states the caller's obligation
plainly:

> Caller MUST leave this as-is (defaulted to `sizeof`). The platform copies
> `min(structSize, its own sizeof)` and defaults the rest — 1.x can append fields
> without breaking compiled callers.

The platform refuses any caller whose `structSize` is below the full 1.0 struct size
(`kSettingsMinSize10` / `kBrowserSettingsMinSize10`, both 12):

> Minimum accepted `structSize` for a caller-supplied `Settings`: the full 1.0 struct
> size. Any caller whose `structSize` claims less than this is either pre-1.0
> (predates `structSize` entirely) or corrupt, and cannot be read.

Consequences for you:

* A plugin compiled against **pre-1.0** MeridianUI headers is refused at
  `RequestAPI` — the log reads `caller Settings rejected — structSize below the 1.0
  minimum (pre-1.0 layout or corrupt); refusing API`, and no API is handed out. The fix
  is to rebuild against the 1.0 headers; there is no compatibility shim.
* The same gate applies per browser to `BrowserSettings` in the settings-taking
  `AddOrGetBrowser` overload; a refused struct fails browser creation with
  `caller BrowserSettings rejected — ...`.
* Never assign `structSize` yourself, and never memset the struct after construction.

---

## 2. Browser lifecycle

### The instance-lifetime contract

Quoted verbatim from the frozen `IBrowser.h` class comment — this is the single rule
that decides where your registration code has to live:

> A browser instance handed out by `IUIPlatformAPI::AddOrGetBrowser`.
>
> Lifetime contract (1.0): every registration on this interface — JS function bindings
> (fire-and-forget, event, and promise) and toggle keys — belongs to THIS browser
> instance. Bindings survive navigation and reload within the instance (the platform
> replays them on load) — except that `LoadBrowserURL` clears the fire-and-forget and
> promise bindings first unless called with `a_clearJSFunctions=false`; event-function
> bindings are unaffected, being re-installed at every page load. A browser that is
> released and re-created is a NEW instance: the consumer must re-register its bindings
> and toggle keys on it.

That `LoadBrowserURL` exception is easy to trip over, because the parameter defaults to
`true`: navigating with `LoadBrowserURL(url)` clears the already-registered
fire-and-forget and promise bindings at the next load start. (Your event function is not
one of them — it is installed from the browser's creation-time information and re-created
with every page's JS context. What a navigation does drop is the *old page's* registered
listener, which dies with its context; the new page calls `addEventListener` again.) Pass
`false` when you navigate and want to keep the cleared kinds:

```cpp
browser->LoadBrowserURL("mod://MyMod/settings.html", /*a_clearJSFunctions=*/false);
```

### Creating a browser

```cpp
void MyMod::OnMeridianReady(Meridian::UI::IUIPlatformAPI* a_api)
{
    // Bindings you want present from the first load (see section 3).
    static Meridian::JS::JSFuncInfo s_onSave{};
    s_onSave.objectName = "MyModUI";
    s_onSave.funcName = "save";
    s_onSave.callbackData.executeInGameThread = true;
    s_onSave.callbackData.callback = [](const char** a_args, int a_argsCount) { /* ... */ };

    Meridian::JS::JSFuncInfo* funcs[] = {&s_onSave};

    Meridian::CEF::IBrowser* browser = nullptr;
    const auto handle = a_api->AddOrGetBrowser("MyMod.MainPanel",
                                               funcs,
                                               static_cast<std::uint32_t>(std::size(funcs)),
                                               "mod://MyMod/index.html",
                                               browser);

    if (handle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle || browser == nullptr)
    {
        spdlog::error("MyMod: failed to create browser");
        return;
    }

    // Store `handle` and `browser`; release the handle when you are done with it.
}
```

Points that matter:

* **`a_browserName` is the identity.** The same name returns the same browser; each
  call returns a *new* handle to it, and the fire-and-forget entries of
  `a_funcInfoArr` are (re-)applied to the existing instance. Namespace the name with
  your mod's prefix — it is a process-wide key shared with every other MeridianUI
  consumer.
* **Check both outputs.** Failure is signalled by
  `IUIPlatformAPI::InvalidBrowserRefHandle` (0) *and* `a_outBrowser == nullptr`. The
  platform refuses browser creation during shutdown and before it is initialized.
* **`a_funcInfoArr` is an array of pointers** (`JSFuncInfo* const*`) plus a count; pass
  `nullptr, 0` for no bindings. Every supplied entry and its object/function names must
  be non-null and non-empty. The platform copies those names during the call. Your
  `callback` function pointers, by contrast, must stay valid for as long as the browser
  can call them.
* **`a_browserName` and `a_startUrl` must be non-null and non-empty.** The secure
  release default requires a pinned `mod://` URL (section 4).
* The second overload takes a `Meridian::UI::BrowserSettings*` before the out-browser
  parameter and applies it **to a newly created browser only** — it does nothing when
  the named browser already exists.

### Waiting for the browser and the page

`IsBrowserReady()` reports that the CEF browser object exists; `IsPageLoaded()` reports
that the main frame has finished loading. Both are cheap polls — a non-blocking check
once per frame from the game thread is perfectly legitimate. What you must never do is
*block* the game thread on them: no spin loop inside an SKSE task or a hook. A wait
belongs on your own thread:

```cpp
m_worker = std::jthread([this](std::stop_token a_stop) {
    while (!a_stop.stop_requested() && !m_browser->IsPageLoaded())
    {
        std::this_thread::sleep_for(100ms);
    }
    if (a_stop.stop_requested())
    {
        return;
    }

    m_browser->ExecEventFunction("on:ready", "");
});
```

Give the wait a deadline if a failed load must not hang your worker — the platform's
own fixture uses `std::chrono::steady_clock::now() + 30s` and logs an error if the page
has still not loaded.

You do not have to wait before calling into `IBrowser`: `LoadBrowserURL`,
`ExecuteJavaScript`, `AddFunctionCallback`, `AddPromiseFunctionCallback`, and
`SetBrowserFocused` all cache their work while the page is unloaded and replay it at
the next load.

### Releasing and re-creating

`ReleaseBrowserHandle(handle)` drops one reference:

> Releases browser if no one has the handle

The browser is destroyed when its **last** handle is released. Do this on the game
thread (an `SKSE::GetTaskInterface()->AddTask` is the normal vehicle), then treat the
`IBrowser*` you were holding as dangling and clear it.

A re-created browser is a new instance, so everything instance-scoped has to be armed
again:

```cpp
SKSE::GetTaskInterface()->AddTask([this]() {
    if (m_handle != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
    {
        m_api->ReleaseBrowserHandle(m_handle);
        m_handle = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        m_browser = nullptr;
    }

    // m_funcs / m_funcCount / m_startUrl: the JSFuncInfo array and start URL
    // you built when the browser was first created (see "Creating a browser").
    m_handle = m_api->AddOrGetBrowser("MyMod.MainPanel", m_funcs, m_funcCount, m_startUrl, m_browser);
    if (m_handle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle || m_browser == nullptr)
    {
        return;
    }

    // Toggle keys live per browser instance — re-arm them.
    m_browser->ToggleBrowserFocusByKeys(RE::BSKeyboardDevice::Keys::kF6, 0);
    // Promise bindings live per browser instance too — re-register them
    // (see section 3.3 for RegisterPromiseNatives).
    MyMod::RegisterPromiseNatives(m_browser);
});
```

(Fire-and-forget bindings passed in `a_funcInfoArr` are re-applied by
`AddOrGetBrowser` itself; toggle keys and promise bindings are not — they have no
array parameter.)

### Shutdown

Register a shutdown callback as soon as you have the API:

```cpp
a_api->RegisterOnShutdown(&MyMod::OnMeridianShutdown);  // void(*)()
```

The header's rule is: *"After this callback, you should stop using any browser."* Stop
your worker threads and drop your `IBrowser*` pointers there. You do not have to
release your handles to let the platform close cleanly — the shutdown coordinator
closes and drains browsers that consumers still hold. If you register a callback after
shutdown has already begun, it is invoked immediately.

---

## 3. JS bindings

Three kinds of binding exist in 1.0. All three are per browser instance (section 2),
and all three deliver their arguments the same way: **every element of `a_args` is
already a valid JSON document**. A JS string arrives quoted (`"str"`), a number as
`2.33`, an object or array as its JSON text, `null` as `null`. Concatenating them with
commas inside `[...]` produces a valid JSON array without re-encoding.

`executeInGameThread` decides where your callback runs: `true` (the default) queues it
onto the game thread through `SKSE::GetTaskInterface()`; `false` runs it inline on the
CEF thread that received the call. Use `true` for anything touching game state, `false`
for pure computation or work you are going to hand off yourself.

### 3.1 Fire-and-forget bindings

`JSFuncInfo` + `AddFunctionCallback` (or the `a_funcInfoArr` parameter of
`AddOrGetBrowser`). The JS function returns `undefined`; there is no return channel.

```cpp
Meridian::JS::JSFuncInfo notify{};
notify.objectName = "MyModUI";
notify.funcName = "notify";
notify.callbackData.executeInGameThread = false;
notify.callbackData.callback = [](const char** a_args, int a_argsCount) {
    std::string argsStr;
    for (auto i = 0; i < a_argsCount; ++i)
    {
        argsStr += fmt::format("{}{}", (i > 0 ? ", " : ""), a_args[i]);
    }
    spdlog::info("MyModUI.notify: {}", argsStr);
};

browser->AddFunctionCallback(notify);
```

```html
<script>
  // arrives as: 1, 2.33, "str", [1,2,3], {"k1":1,"k2":null}
  MyModUI.notify(1, 2.33, "str", [1, 2, 3], { "k1": 1, "k2": null });
</script>
```

`RemoveFunctionCallback("MyModUI", "notify")` (or the `JSFuncInfo` overload) unbinds
one; after removal the page sees a plain `TypeError` on the next call, so guard
optional calls with `if (MyModUI.notify)` or a `try`/`catch`.

### 3.2 Event functions (C++ → JS)

An event function is the reverse direction: the page registers a listener, and your
plugin pushes data to it. Declare it as a `JSFuncInfo` with
`callbackData.isEventFunction = true` (no `callback` needed) and pass it **in the
`AddOrGetBrowser` array at creation time** — the event function is captured when the
browser is created, and `AddFunctionCallback` cannot add one later.

```cpp
// a_api: the IUIPlatformAPI* from section 1. startUrl: your page's URL.
Meridian::JS::JSFuncInfo addListener{};
addListener.objectName = "MyModUI";
addListener.funcName = "addEventListener";
addListener.callbackData.isEventFunction = true;

Meridian::JS::JSFuncInfo* funcs[] = {&addListener /*, other bindings */};

Meridian::CEF::IBrowser* browser = nullptr;
const auto handle = a_api->AddOrGetBrowser("MyMod.MainPanel", funcs, 1, startUrl, browser);

// later, from anywhere:
browser->ExecEventFunction("on:message", "player entered Whiterun");
```

```html
<script>
  MyModUI.addEventListener("on:message", (data) => {
    document.getElementById("log").textContent = data;   // data is a string
  });
</script>
```

The JS side takes exactly two arguments — event name (string) and callback (function);
anything else throws. The callback receives the `a_data` you passed as a **string**; if
you want structured data, send JSON text and `JSON.parse` it in the page. One listener
per event name per browser: registering the same name again replaces the previous
callback.

### 3.3 Promise natives (JS → C++ → JS)

`AddPromiseFunctionCallback` registers a JS function that returns a `Promise`. The
header:

> Registers a JS function `window.<objectName>.<funcName>(...)` that returns a Promise.
> The native callback receives the arguments as JSON strings plus a one-shot resolver
> it may settle from any thread, immediately or later — including after the browser has
> closed (a post-close settle is a safe no-op). Fire-and-forget bindings registered via
> `AddFunctionCallback` are unaffected and keep returning undefined.

Promise bindings have no array parameter on `AddOrGetBrowser` — that overload only
accepts fire-and-forget `JSFuncInfo` entries — so register them directly on the
browser:

```cpp
void MyMod::RegisterPromiseNatives(Meridian::CEF::IBrowser* a_browser)
{
    Meridian::JS::JSPromiseFuncInfo getPlayerName;
    getPlayerName.objectName = "MyModUI";
    getPlayerName.funcName = "getPlayerName";
    getPlayerName.executeInGameThread = true;   // reads game state
    getPlayerName.callback = [](const char** a_args, int a_argsCount, Meridian::JS::IJSPromiseResolver* a_resolver) {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (player == nullptr)
        {
            a_resolver->Reject("player unavailable");
            return;
        }
        a_resolver->Resolve(fmt::format(R"({{"name":"{}"}})", player->GetName()).c_str());
    };
    a_browser->AddPromiseFunctionCallback(getPlayerName);

    Meridian::JS::JSPromiseFuncInfo slowWork;
    slowWork.objectName = "MyModUI";
    slowWork.funcName = "slowWork";
    slowWork.executeInGameThread = false;
    slowWork.callback = [](const char** a_args, int a_argsCount, Meridian::JS::IJSPromiseResolver* a_resolver) {
        // Capture ONLY the resolver (and owned copies of the data you need) —
        // never `this`, never anything that can be destroyed first.
        std::thread([a_resolver]() {
            std::this_thread::sleep_for(1s);
            a_resolver->Resolve(R"({"done":true})");
        }).detach();
    };
    a_browser->AddPromiseFunctionCallback(slowWork);
}
```

```html
<script>
  async function refresh() {
    try {
      const result = await window.MyModUI.getPlayerName();
      document.getElementById("name").textContent = result.name;
    } catch (e) {
      document.getElementById("name").textContent = "failed: " + e.message;
    }
  }
</script>
```

The resolved value arrives in JS already parsed from the JSON payload you passed;
a rejection surfaces as an `Error` whose `message` is your `a_errorMessage`. Always
`await` inside `try`/`catch` — an unhandled rejection is an unhandled rejection like
any other.

**Resolver contract** (`IJSPromiseResolver`, quoted from `JSTypes.h`):

> Settle the JS promise with a JSON payload (object, array, string, number, bool, or
> null — parsed on the JS side). One-shot: the first `Resolve` or `Reject` wins; later
> calls are logged no-ops. Callable from any thread, at any time, including after the
> browser has closed (then it is a safe no-op). A payload that is not valid JSON
> rejects the promise instead; `nullptr` resolves with null.

The lifetime rule that follows from this: **the platform retains every resolver for the
process lifetime**, so a thread you spawn inside a promise callback may run long after
the object that spawned it is gone. Capture only the resolver pointer and owned copies
of the data you need — never `this`, never a pointer into your plugin's state, never
anything a browser release could destroy first.

**There is no remove API for promise bindings in 1.0**:

> There is NO remove API for promise bindings in 1.0 (the renderer keeps a reserved
> handler for a future one). To retire a binding, load a page that no longer calls it,
> or re-create the browser.

---

## 4. The `mod://` scheme

### Layout

Ship your UI assets under a folder named for your mod:

```
Data\MeridianUI\<YourModName>\index.html
Data\MeridianUI\<YourModName>\app.js
Data\MeridianUI\<YourModName>\assets\logo.png
```

and address them as:

```
mod://<YourModName>/index.html
mod://<YourModName>/assets/logo.png
```

The host component is your mod folder name; everything after the first `/` is the
relative path under it. The scheme is registered as standard, secure, CORS-enabled and
fetch-enabled, so `fetch()` and `XHR` against your own `mod://` assets work from a
`mod://` page.

Recognized extensions are served with a real MIME type (`html`, `htm`, `js`, `mjs`,
`css`, `json`, `png`, `jpg`, `jpeg`, `gif`, `svg`, `webp`, `woff`, `woff2`, `ttf`,
`wasm`, `txt`); anything else is served as `application/octet-stream`.

### Rules

* **Explicit files only.** There is no directory index and no default-document
  fallback: `mod://MyMod/` or `mod://MyMod/assets` will not serve `index.html`. Always
  name the file.
* **Query strings are stripped** before the path is resolved, so the cache-busting
  `?v=123` idiom is safe: `mod://MyMod/app.js?v=3` serves `app.js` with the right MIME
  type.
* **Traversal is rejected**, by two independent layers. The string layer rejects
  empty/`.`/`..` segments, segments containing `:`, segments ending in `.` or a space
  (Windows silently strips those at open time), malformed or double percent-escapes,
  embedded NULs, and a host that is empty, contains a separator, or has the same
  trailing-dot/space problem; backslashes are normalized to `/` first. The filesystem
  layer then canonicalizes both your mod root and the resolved file through the OS and
  refuses anything that does not land inside the root, so junctions and short names
  cannot smuggle a path past the string check.
* A rejected or missing URL produces a 404 and a throttled
  `ModSchemeHandler: 404 for "<url>"` warning in the log — the first place to look when
  a page comes up blank.

### Release browsers require `mod://`

Every browser defaults to trusted local content: its first page must be a valid
`mod://` URL, that mod host is pinned, and Meridian rejects remote, `file://`,
cross-mod, redirect, subframe, and subresource navigation. `data:` and `blob:`
resources created by the trusted document remain available.

Ordinary remote content is a development-only exception enabled globally through
`AllowRemoteContent`. It applies only to browsers without native bindings. Adding a
fire-and-forget, event, or promise native binding immediately restores the mandatory
`mod://` boundary; a remote document can never expose Skyrim callbacks. The opt-in is
limited to HTTP/HTTPS and never enables `file://`, FTP, or arbitrary custom schemes.

---

## 5. Settings and INI

### `Settings` (global, one per install)

Passed once with `RequestAPI`; the **first** consumer's settings initialize the
library, so treat these as a request, not a guarantee — another plugin may have already
initialized MeridianUI with its own.

| Field | Default | Meaning |
| --- | --- | --- |
| `structSize` | `sizeof(Settings)` | leave it alone (section 1) |
| `remoteDebuggingPort` | `0` | CEF remote debugging. Disabled by default; a valid `[1024, 65535]` port is an explicit local-development opt-in |
| `nativeMenuLangSwitching` | `true` | allow keyboard-language switching in native game menus (console, race menu, ...) |
| `rendererType` | `RendererType::RingBuffer` | see below |
| `allowRemoteContent` | `false` | development-only opt-in for ordinary HTTP/HTTPS pages in browsers without native bindings |

### `BrowserSettings` (per new browser)

| Field | Default | Meaning |
| --- | --- | --- |
| `structSize` | `sizeof(BrowserSettings)` | leave it alone |
| `frameRate` | `60` | desired frame rate for that browser |
| `reservPad` | `0` | reserved |

Both structs follow the same versioning rule: leave `structSize` defaulted, and 1.x
releases can append fields without breaking your compiled plugin — the platform copies
`min(structSize, its own sizeof)` and defaults the rest. Anything below the 1.0 size is
refused outright (section 1).

### Renderer choice

From `Settings.h`:

* **`RingBuffer` (default)** — triple-buffered shared-texture ring; honors per-browser
  rect/scale/z geometry. *"Use this."*
* **`SyncCopy`** — synchronous full-copy renderer; slower, full geometry parity since
  1.0. Fallback/diagnostic path, selectable via INI.
* **`DeferredContext`** — legacy pre-1.0 copy renderer (D3D11 deferred context),
  fullscreen only — it ignores rect/scale geometry. Kept for compatibility and
  diagnostics, and deliberately **not** selectable via `MeridianUI.ini`; it exists in
  the API enum only.

Do not set a renderer in your plugin unless you have a specific reason. Renderer choice
belongs to the user, through the INI.

### `MeridianUI.ini` — the user's override file

MeridianUI ships a commented template as `Data\SKSE\Plugins\MeridianUI.ini.disabled`,
next to the plugin DLL. The user renames it to `MeridianUI.ini` (dropping the
`.disabled` suffix) to activate it. Every entry is optional; commented entries keep
their defaults.

**INI wins.** Any value set there overrides what consumer plugins passed in `Settings`.
When an override is applied, the log names the keys: `MeridianUI.ini overrode: ...`.

```ini
[General]
; Renderer for browser surfaces: RingBuffer (default) or SyncCopy.
;RendererType=RingBuffer

; Allow language switching in native game menus (console, race menu, ...).
; Default: true
;NativeMenuLangSwitching=true

; Development-only remote pages for browsers without native bindings.
; Default: false
;AllowRemoteContent=false

[Debug]
; Master switch for the CEF remote debugger. Setting false forces the port
; off and takes precedence over RemoteDebuggingPort below.
; Default: false. Enable only for local development.
;RemoteDebuggingEnabled=false

; Remote debugger port (http://localhost:<port>). Valid range 1024-65535;
; 0 disables. Default: 0. Set both values explicitly for local development.
;RemoteDebuggingPort=9009

; MeridianUI log verbosity: trace, debug, info, warn, error.
; Default: info
;LogLevel=info
```

Key notes:

* `RendererType` accepts only `RingBuffer` and `SyncCopy` (case-insensitive).
  `DeferredContext` is not accepted here by design.
* Booleans accept `1`/`0`, `true`/`false`, `yes`/`no`, case-insensitively.
* `RemoteDebuggingEnabled=false` forces the port off regardless of
  `RemoteDebuggingPort`.
* `AllowRemoteContent=false` is the supported release setting. Enabling it does not
  permit native bindings in remote documents or enable local-file/custom-scheme access.
* `LogLevel` accepts exactly `trace`, `debug`, `info`, `warn`, `error` — no `critical`,
  no `off`.
* A malformed value is ignored with a
  `MeridianUI.ini: malformed value for "<key>"; ignoring` warning rather than failing
  the load.

When debugging your own UI, set `RemoteDebuggingEnabled=true`, choose a port such as
`9009`, and use `LogLevel=debug`. Disable the debugger again for normal play and
release packaging. MeridianUI logs to `MeridianUI.log` in the standard SKSE log directory;
the CEF child process logs alongside it as `MeridianCEFSubprocess.exe.log`.

---

## 6. Focus, input, toggle keys

### Focus semantics

Focus is arbitrated globally: **exactly one** MeridianUI browser holds focus at a time,
across all consumers. `SetBrowserFocused(true)` claims it and deposes whoever held it;
`SetBrowserFocused(false)` releases it. `IsBrowserFocused()` reports whether *this*
browser is the current owner.

What focus buys the owner:

* **Keyboard and mouse.** Mouse-move, mouse-button, and keyboard events are delivered
  to the focused browser and swallowed so the game does not also act on them. An
  unfocused browser receives no input at all.
* **Controller input.** 1.1.0 does not synthesize gamepad navigation. Unsupported
  controller/device events pass through to Skyrim rather than being swallowed.
* **The cursor.** Claiming focus resets the cursor to the arrow and opens the
  platform's transient focus menu; releasing it restores the vanilla cursor movie.
  Mouse clicks are hit-tested against the browser's on-screen rect, so a click outside
  a small widget's rect is not consumed by it.
* Player look/move input is zeroed on the focus grant, so the camera does not drift
  while the user is typing into a page.

**Unfocused browsers still render.** Focus governs input and cursor only, never
visibility: a HUD widget can be on screen and updating while the player controls the
game normally. `SetBrowserVisible(false)`, by contrast, also releases focus.

If you call `SetBrowserFocused` before the page has loaded, the value is cached and
applied at load end — you do not need to wait.

### Toggle keys

```cpp
browser->ToggleBrowserFocusByKeys(RE::BSKeyboardDevice::Keys::kF6, 0);   // focus on/off
browser->ToggleBrowserVisibleByKeys(RE::BSKeyboardDevice::Keys::kF7, 0); // visibility on/off
```

Both take up to two DirectInput scan codes (see `RE::BSKeyboardDevice::Keys`); the
second is the optional second half of a chord. **Pass zeros to disable.** Registration
is per browser instance — after a release/re-create you must arm them again (section
2).

The 1.0 contract, quoted from `IBrowser.h` (both toggle methods carry it):

> Toggle chords are evaluated for EVERY browser before any focused browser can swallow
> the event (1.0 contract): while a registered chord is held, any button-down that
> completes or re-satisfies it is consumed and never reaches a page; key-ups and lone
> chord-halves are not intercepted.

So your focus and visibility hotkeys work regardless of which browser currently holds
focus — another mod's focused browser cannot make your toggle key inert. Pick uncommon
keys anyway: a completed chord is consumed globally, including from the game.

### Geometry

`SetBrowserRect(x, y, w, h)` sets the on-screen destination in pixels (a browser is
fullscreen until you call it), `GetBrowserRect` reads it back,
`SetBrowserResolutionScale(s)` renders at rect x scale and stretches to the rect
(clamped to `(0, 2]`, default `1.0` — `0.5` quarters a widget's pixel cost), and
`SetBrowserZOrder(z)` puts higher z on top with ties broken by creation order
(default `0`). Input hit-testing follows the rect, so geometry and input stay
consistent. These are honored by `RingBuffer` and `SyncCopy`, and ignored by the legacy
`DeferredContext` renderer (section 5).

---

## 7. Coexistence and limitations

### PrismaUI

PrismaUI is the other Chromium-in-Skyrim framework in wide use. Both frameworks hook
`CursorMenu`. Cursor ownership is therefore focus-scoped: while a Meridian browser owns
focus, Meridian suppresses the vanilla cursor after PrismaUI's per-frame visibility
update and draws its own cursor after all Meridian browser layers. When the final
Meridian focus owner releases, vanilla/Prisma cursor behavior is restored. Merely having
`PrismaUI.dll` installed does not change ownership.

MeridianUI detects PrismaUI at startup and logs a line naming this document:

```
PublicAPIController: PrismaUI detected in-process — focus-scoped cursor arbitration active
```

This is cursor ownership compatibility, not cross-framework focus negotiation. Release
focus when your page is dismissed rather than leaving a browser focused in the
background. The focus-scoped cursor fix's in-game Prisma A/B gate is still `NOT RUN`.

### Runtime support

* **AE 1.6.1170 / SKSE 2.2.6** — earlier 1.0 lifecycle evidence passed; the final 1.1.1
  regression gate is `NOT RUN`.
* **SE 1.5.97 / SKSE 2.0.20** — compiled by the shared NG target; the in-game gate is
  `NOT RUN`.
* **VR** — unsupported. It requires a dedicated stereo compositor, relocations, and
  controller-ray input rather than a build-flag change.

### Known 1.0 limitations

* **No remove API for promise bindings** — retire one by loading a page that no longer
  calls it, or by re-creating the browser (section 3.3).
* **Event functions are creation-time only** — they must be passed in the
  `AddOrGetBrowser` array and cannot be added later (section 3.2).
* **Everything is per browser instance** — bindings and toggle keys must be
  re-registered after a release/re-create (section 2).
* **`LoadBrowserURL` clears fire-and-forget and promise bindings by default** — pass
  `a_clearJSFunctions = false` to keep them across a navigation; event bindings are
  unaffected (section 2).
* **`DeferredContext` ignores geometry** and is API-only, not INI-selectable
  (section 5).
* **Pre-1.0 consumers are refused, not degraded** — rebuild against the 1.0 headers
  (section 1).
* **CEF sandboxing remains disabled** in the injected SKSE architecture. Consequently,
  the release default restricts every browser to one trusted `mod://` host. Remote
  content is an explicit development-only opt-in and is never permitted to expose
  native bindings.
