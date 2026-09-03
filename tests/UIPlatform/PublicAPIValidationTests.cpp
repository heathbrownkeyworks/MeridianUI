#include "Controllers/PublicAPIValidation.h"

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
    using Meridian::Controllers::PublicAPIValidation::Error;
    using Meridian::Controllers::PublicAPIValidation::ValidateBrowserRequest;

    Expect(ValidateBrowserRequest(nullptr, nullptr, 0, "mod://Example/index.html") == Error::NullBrowserName,
           "null browser name is rejected");
    Expect(ValidateBrowserRequest("", nullptr, 0, "mod://Example/index.html") == Error::EmptyBrowserName,
           "empty browser name is rejected");
    Expect(ValidateBrowserRequest("Example", nullptr, 0, nullptr) == Error::NullStartUrl,
           "null start URL is rejected");
    Expect(ValidateBrowserRequest("Example", nullptr, 0, "") == Error::EmptyStartUrl,
           "empty start URL is rejected");
    Expect(ValidateBrowserRequest("Example", nullptr, 1, "mod://Example/index.html") == Error::MissingFunctionArray,
           "nonzero function count requires an array");

    Meridian::JS::JSFuncInfo valid{"native", "close", {}};
    Meridian::JS::JSFuncInfo missingObject{nullptr, "close", {}};
    Meridian::JS::JSFuncInfo missingFunction{"native", nullptr, {}};
    Meridian::JS::JSFuncInfo* nullEntry[] = {nullptr};
    Meridian::JS::JSFuncInfo* missingObjectEntry[] = {&missingObject};
    Meridian::JS::JSFuncInfo* missingFunctionEntry[] = {&missingFunction};
    Meridian::JS::JSFuncInfo* validEntry[] = {&valid};

    Expect(ValidateBrowserRequest("Example", nullEntry, 1, "mod://Example/index.html") == Error::NullFunctionEntry,
           "null function entry is rejected");
    Expect(ValidateBrowserRequest("Example", missingObjectEntry, 1, "mod://Example/index.html") == Error::InvalidFunctionName,
           "null JS object name is rejected");
    Expect(ValidateBrowserRequest("Example", missingFunctionEntry, 1, "mod://Example/index.html") == Error::InvalidFunctionName,
           "null JS function name is rejected");
    Expect(ValidateBrowserRequest("Example", validEntry, 1, "mod://Example/index.html") == Error::None,
           "well-formed request is accepted");
    Expect(ValidateBrowserRequest("Example", validEntry, 0, "mod://Example/index.html") == Error::None,
           "a supplied array with zero entries is harmless");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " PublicAPIValidation test(s) failed\n";
        return 1;
    }
    std::cout << "All PublicAPIValidation tests passed\n";
    return 0;
}
