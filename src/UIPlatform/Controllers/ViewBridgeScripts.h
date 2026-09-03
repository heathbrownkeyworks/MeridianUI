#pragma once

#include <string>
#include <string_view>

namespace Meridian::Controllers::ViewBridgeScripts
{
    std::string BuildBootstrap(std::string_view a_token);
    std::string BuildListener(std::string_view a_listenerName);
}
