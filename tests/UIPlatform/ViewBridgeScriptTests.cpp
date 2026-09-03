#include "Controllers/ViewBridgeScripts.h"

#include <iostream>
#include <string>
#include <string_view>

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

    std::size_t Count(std::string_view a_value, std::string_view a_needle)
    {
        std::size_t count = 0;
        for (std::size_t offset = 0;
             (offset = a_value.find(a_needle, offset)) != std::string_view::npos;
             offset += a_needle.size())
        {
            ++count;
        }
        return count;
    }
}

int main()
{
    using namespace Meridian::Controllers::ViewBridgeScripts;

    const auto bootstrap = BuildBootstrap("tok\"en");
    Expect(bootstrap.starts_with("(function(token){") && bootstrap.ends_with(");"),
           "bootstrap is emitted as a complete invocation");
    Expect(bootstrap.find("setTimeout(initialize, 16)") != std::string::npos,
           "bootstrap retries when the native CEF object is late");
    Expect(bootstrap.find("__meridianViewPendingListeners") != std::string::npos,
           "bootstrap drains listeners queued before native binding");
    Expect(bootstrap.find("__meridianViewBoundListeners") != std::string::npos,
           "bootstrap tracks bound listeners to prevent duplicates");
    Expect(bootstrap.find("pending.splice(0)") != std::string::npos,
           "bootstrap consumes the pending listener queue");
    Expect(Count(bootstrap, "dispatch(token, '__meridian_dom_ready', '')") == 1,
           "bootstrap emits the DOM-ready callback exactly once");
    Expect(bootstrap.find("__meridian_text_input") != std::string::npos,
           "bootstrap reports editable focus through the internal listener");
    Expect(bootstrap.find("TEXTAREA") != std::string::npos &&
               bootstrap.find("INPUT") != std::string::npos &&
               bootstrap.find("isContentEditable") != std::string::npos,
           "bootstrap recognizes standard editable elements");
    Expect(bootstrap.find("'datetime-local': true") != std::string::npos &&
               bootstrap.find("number: true") != std::string::npos,
           "bootstrap includes structured keyboard-editable input types");
    Expect(bootstrap.find("document.activeElement") != std::string::npos,
           "bootstrap derives text-input state from the final active element");
    Expect(bootstrap.find("addEventListener('focusin', reportTextInput, true)") != std::string::npos,
           "bootstrap observes focus entering descendants in capture phase");
    Expect(bootstrap.find("window.setTimeout(reportTextInput, 0)") != std::string::npos,
           "bootstrap defers focus-out reporting across editable-to-editable transitions");
    Expect(bootstrap.find(R"JS("tok\"en")JS") != std::string::npos,
           "bootstrap JSON-escapes the view token");

    const auto listener = BuildListener("close\"button");
    Expect(listener.starts_with("(function(name){") && listener.ends_with(");"),
           "listener is emitted as a complete invocation");
    Expect(listener.find("typeof window.__meridianViewBind === 'function'") != std::string::npos,
           "listener binds immediately when the bridge is ready");
    Expect(listener.find("__meridianViewPendingListeners") != std::string::npos,
           "listener queues itself when the bridge is not ready");
    Expect(listener.find("pending.indexOf(name) === -1") != std::string::npos,
           "listener queue rejects duplicate registrations");
    Expect(listener.find(R"JS("close\"button")JS") != std::string::npos,
           "listener JSON-escapes its callback name");

    if (g_failures == 0)
    {
        std::cout << "All ViewBridgeScript tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
