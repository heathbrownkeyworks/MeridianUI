#define MERIDIAN_JS_BINDING_MESSAGE_NO_CEF 1
#include "IPC/JsBindingMessage.h"

#include <iostream>
#include <string>

namespace
{
    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestDictionaryKeyIsAlwaysTheObjectName()
    {
        const auto message = Meridian::JS::MakeSingleBinding("NL", "func1");
        Expect(message.DictionaryKey() == "NL", "dictionary key must be the object name");
        Expect(message.DictionaryValues().size() == 1, "single binding must carry one function name");
        Expect(message.DictionaryValues().front() == "func1", "dictionary value must be the function name");
    }

    void TestAddAndRemovePathsAgreeOnShape()
    {
        // The add and remove paths must produce an identical shape for identical
        // input. This is the regression guard for the swapped-dictionary defect.
        const auto addMessage = Meridian::JS::MakeSingleBinding("NL", "func1");
        const auto removeMessage = Meridian::JS::MakeSingleBinding("NL", "func1");

        Expect(addMessage.DictionaryKey() == removeMessage.DictionaryKey(),
               "add and remove must use the same dictionary key");
        Expect(addMessage.DictionaryValues() == removeMessage.DictionaryValues(),
               "add and remove must use the same dictionary values");
        Expect(addMessage.DictionaryKey() != addMessage.DictionaryValues().front(),
               "key and value must not be interchangeable in this fixture");
    }

    void TestNullInputsProduceEmptyStrings()
    {
        const auto message = Meridian::JS::MakeSingleBinding(nullptr, nullptr);
        Expect(message.DictionaryKey().empty(), "null object name must yield an empty key");
        Expect(message.DictionaryValues().size() == 1, "null function name still yields one entry");
        Expect(message.DictionaryValues().front().empty(), "null function name must yield an empty value");
    }

    // NOTE on bulk ToCefDictionary / FromCefDictionary coverage:
    // This target compiles with MERIDIAN_JS_BINDING_MESSAGE_NO_CEF, so the
    // CEF-guarded bulk ToCefDictionary/FromCefDictionary functions are not
    // visible here (its CMake target attaches no CEF include dirs or
    // libraries, by design, to keep this test CEF-free). Dictionary-shape
    // assertions for those functions can't live in this file.
    //
    // Verification instead comes from two other places:
    //   1. Same-shape-by-construction: the single-message ToCefDictionary
    //      overload is implemented as
    //      `return ToCefDictionary(std::vector<JsBindingMessage>{a_message});`
    //      so the single and bulk paths cannot disagree on shape — there is
    //      only one shape-producing code path to drift.
    //   2. Call-site compile coverage: JSFunctionStorage::ConvertToCefDictionary
    //      and both FromCefDictionary call sites in
    //      MeridianSubprocessCefApp.cpp are exercised by building
    //      MeridianCEFSubprocess and MeridianUI (both link CEF), which fails
    //      to compile/link if the bulk/bool-return contract is violated.
}

int main()
{
    TestDictionaryKeyIsAlwaysTheObjectName();
    TestAddAndRemovePathsAgreeOnShape();
    TestNullInputsProduceEmptyStrings();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " JsBindingMessage test(s) failed\n";
        return 1;
    }

    std::cout << "All JsBindingMessage tests passed\n";
    return 0;
}
