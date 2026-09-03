#pragma once

#include "LocalTestPage.h"

using namespace Meridian::UI::TestCase;

namespace Meridian::UI::TestCase
{
    static std::shared_ptr<LocalTestPage> s_localTestPage = nullptr;

    /// <summary>
    /// These tests are just for demonstration of the concept, the code is terrible
    /// </summary>
    /// <param name="a_api"></param>
    void StartTests(Meridian::UI::IUIPlatformAPI* a_api)
    {
        s_localTestPage = std::make_shared<LocalTestPage>();
        s_localTestPage->Start(a_api);
    }
}
