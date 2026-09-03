#pragma once

#include "API.h"
#include <SKSE/SKSE.h>

namespace Meridian::UI::SKSELoader
{
    using APIReadyFunc_t = std::function<void(Meridian::UI::IUIPlatformAPI*)>;

    class LoaderData
    {
    public:
        static inline bool s_canUseAPI = false;
        static inline APIReadyFunc_t s_apiReadyCallback = nullptr;
    };

    inline void ProcessSKSEMessage(const SKSE::MessagingInterface::Message* a_msg, Meridian::UI::Settings* settings = nullptr)
    {
        if (std::strcmp(a_msg->sender, "SKSE") == 0)
        {
            switch (a_msg->type)
            {
            case SKSE::MessagingInterface::kPostPostLoad:
                // All plugins are loaded. Request lib version.
                SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::RequestVersion, nullptr, 0, Meridian::UI::LibVersion::PROJECT_NAME);
                break;
            case SKSE::MessagingInterface::kInputLoaded:
                if (LoaderData::s_canUseAPI)
                {
                    Meridian::UI::Settings defaultSettings{};
                    if (settings == nullptr)
                    {
                        settings = &defaultSettings;
                    }

                    // API version is ok. Request interface.
                    SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::RequestAPI, settings, sizeof(*settings), Meridian::UI::LibVersion::PROJECT_NAME);
                }
                break;
            default:
                break;
            }
        }
        else if (std::strcmp(a_msg->sender, Meridian::UI::LibVersion::PROJECT_NAME) == 0)
        {
            switch (a_msg->type)
            {
            case Meridian::UI::APIMessageType::ResponseVersion: {
                const auto versionInfo = reinterpret_cast<Meridian::UI::ResponseVersionMessage*>(a_msg->data);
                spdlog::info("MeridianUI loader: installed version: {}.{}", Meridian::UI::LibVersion::GetMajorVersion(versionInfo->libVersion), Meridian::UI::LibVersion::GetMinorVersion(versionInfo->libVersion));

                const auto majorAPIVersion = Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion);
                const auto minorAPIVersion = Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion);
                // Different major version can cause serious compatibility issues. Older minor version may have missing methods
                if (majorAPIVersion != Meridian::UI::APIVersion::MAJOR || minorAPIVersion < Meridian::UI::APIVersion::MINOR)
                {
                    LoaderData::s_canUseAPI = false;
                    spdlog::error("MeridianUI loader: can't use this API version. We have {}.{}, but {}.{} is installed",
                                  Meridian::UI::APIVersion::MAJOR,
                                  Meridian::UI::APIVersion::MINOR,
                                  Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion),
                                  Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion));
                }
                else
                {
                    LoaderData::s_canUseAPI = true;
                    spdlog::info("MeridianUI loader: API version is ok. Our version {}.{}, installed {}.{}",
                                 Meridian::UI::APIVersion::MAJOR,
                                 Meridian::UI::APIVersion::MINOR,
                                 Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion),
                                 Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion));
                }
                break;
            }
            case Meridian::UI::APIMessageType::ResponseAPI: {
                auto api = reinterpret_cast<Meridian::UI::ResponseAPIMessage*>(a_msg->data)->API;
                if (api == nullptr)
                {
                    spdlog::error("API is nullptr");
                    break;
                }

                LoaderData::s_apiReadyCallback(api);
                break;
            }
            default:
                break;
            }
        }
    }

    inline void GetUIPlatformAPIWithVersionCheck(APIReadyFunc_t a_apiReadyCallback)
    {
        LoaderData::s_apiReadyCallback = a_apiReadyCallback;

        SKSE::GetMessagingInterface()->RegisterListener(Meridian::UI::LibVersion::PROJECT_NAME, [](SKSE::MessagingInterface::Message* a_msg) {
            ProcessSKSEMessage(a_msg);
        });
    }
}
