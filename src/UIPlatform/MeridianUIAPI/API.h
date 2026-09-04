// SPDX-License-Identifier: MIT

#pragma once

#include "Version.h"
#include "JSTypes.h"
#include "IBrowser.h"
#include "Settings.h"

namespace Meridian::UI
{
    class IUIPlatformAPI
    {
    public:
        using BrowserRefHandle = std::uint32_t;
        static constexpr BrowserRefHandle InvalidBrowserRefHandle = 0;
        using OnShutdownFunc_t = void (*)();

    public:
        virtual ~IUIPlatformAPI() = default;

        /// <summary>
        /// Add or get browser
        /// </summary>
        /// <param name="a_browserName">Non-null, non-empty unique name; returns the same browser for the same name</param>
        /// <param name="a_funcInfoArr">Function info array for binding to the browser's JS. Pass nullptr only when a_funcInfoArrSize is zero. Every supplied entry and its object/function names must be non-null and non-empty.</param>
        /// <param name="a_funcInfoArrSize">Number of entries in a_funcInfoArr</param>
        /// <param name="a_startUrl">Non-null, non-empty URL used when the browser is created</param>
        /// <param name="a_outBrowser">Browser interface</param>
        /// <returns>A handle to release when you no longer need the browser</returns>
        virtual BrowserRefHandle __cdecl AddOrGetBrowser(const char* a_browserName,
                                                         Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                         const std::uint32_t a_funcInfoArrSize,
                                                         const char* a_startUrl,
                                                         Meridian::CEF::IBrowser*& a_outBrowser) = 0;

        /// <summary>
        /// Releases browser if no one has the handle
        /// </summary>
        /// <param name="a_handle"></param>
        /// <returns></returns>
        virtual void __cdecl ReleaseBrowserHandle(BrowserRefHandle a_handle) = 0;

        /// <summary>
        /// Add or get browser with custom settings
        /// </summary>
        /// <param name="a_browserName">Non-null, non-empty unique name; returns the same browser for the same name</param>
        /// <param name="a_funcInfoArr">Function info array for binding to the browser's JS. Pass nullptr only when a_funcInfoArrSize is zero. Every supplied entry and its object/function names must be non-null and non-empty.</param>
        /// <param name="a_funcInfoArrSize">Number of entries in a_funcInfoArr</param>
        /// <param name="a_startUrl">Non-null, non-empty URL used when the browser is created</param>
        /// <param name="a_settings">Additional settings for new browser only</param>
        /// <param name="a_outBrowser">Browser interface</param>
        /// <returns></returns>
        virtual BrowserRefHandle __cdecl AddOrGetBrowser(const char* a_browserName,
                                                         Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                         const std::uint32_t a_funcInfoArrSize,
                                                         const char* a_startUrl,
                                                         Meridian::UI::BrowserSettings* a_settings,
                                                         Meridian::CEF::IBrowser*& a_outBrowser) = 0;

        /// <summary>
        /// Registers a shutdown callback function.
        /// After this callback, you should stop using any browser
        /// </summary>
        /// <param name="a_callback"></param>
        virtual void RegisterOnShutdown(OnShutdownFunc_t a_callback) = 0;
    };

    enum APIMessageType : std::uint32_t
    {
        /// <summary>
        /// Request library and api version. No data is needed.
        /// Call after SKSE::MessagingInterface::kPostPostLoad
        /// </summary>
        RequestVersion = 2250,

        /// <summary>
        /// Response with version info. See "ResponseVersionMessage" struct.
        /// You should check current API version (Meridian::UI::APIVersion::AS_INT) and version in response.
        /// It is not guaranteed that the major versions are compatible. In this case, I recommend not using the library.
        /// </summary>
        ResponseVersion,

        /// <summary>
        /// Request API. First request initializes library. See "RequestAPIMessage" struct.
        /// Call after SKSE::MessagingInterface::kInputLoaded
        /// </summary>
        RequestAPI,

        /// <summary>
        /// Response with API info. See "ResponseAPIMessage" struct.
        /// </summary>
        ResponseAPI,
    };

    struct RequestAPIMessage
    {
        Meridian::UI::Settings settings;
    };

    struct ResponseVersionMessage
    {
        /// <summary>
        /// MeridianUI version
        /// </summary>
        std::uint32_t libVersion = Meridian::UI::LibVersion::AS_INT;

        /// <summary>
        /// MeridianUI API version
        /// </summary>
        std::uint32_t apiVersion = Meridian::UI::APIVersion::AS_INT;
    };

    struct ResponseAPIMessage
    {
        IUIPlatformAPI* API = nullptr;
    };
}
