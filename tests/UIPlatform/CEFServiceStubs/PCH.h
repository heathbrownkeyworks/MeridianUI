#pragma once

// Test doubles for the CEF boundary only. The tests compile the production
// CEFService.cpp, so its locking, retry decisions, and shutdown logic are real.
#include <atomic>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace fmt { using std::format; }
namespace spdlog
{
    template <class... Args> void info(Args&&...) {}
    template <class... Args> void warn(Args&&...) {}
    template <class... Args> void error(Args&&...) {}
}

#define NameOf(name) #name
#define IMPLEMENT_REFCOUNTING(name)
#define DISALLOW_COPY_AND_ASSIGN(name)

template <class T> using CefRefPtr = std::shared_ptr<T>;
using CefString = std::string;
struct CefApp {};
struct CefClient {};
struct CefDictionaryValue {};
struct CefSettings {};
struct CefWindowInfo {};
struct CefBrowserSettings {};
struct CefMainArgs { explicit CefMainArgs(void*) {} };
inline void* GetModuleHandleA(const char*) { return nullptr; }

namespace CefCalls
{
    inline std::atomic<int> initialize{0}, exitCode{0}, registerScheme{0};
    inline std::atomic<int> currentlyOn{0}, postTask{0}, shutdown{0}, browser{0};
    inline bool initializeResult = false;
    inline int resultCode = 38;
    inline int Total()
    {
        return initialize + exitCode + registerScheme + currentlyOn + postTask + shutdown + browser;
    }
}

inline bool CefInitialize(const CefMainArgs&, const CefSettings&, CefRefPtr<CefApp>, void*)
{
    ++CefCalls::initialize;
    return CefCalls::initializeResult;
}
inline int CefGetExitCode() { ++CefCalls::exitCode; return CefCalls::resultCode; }
inline void CefShutdown() { ++CefCalls::shutdown; }
inline constexpr int TID_UI = 0;
inline bool CefCurrentlyOn(int) { ++CefCalls::currentlyOn; return false; }

class CefTask
{
public:
    virtual ~CefTask() = default;
    virtual void Execute() = 0;
};
inline bool CefPostTask(int, CefRefPtr<CefTask> a_task)
{
    ++CefCalls::postTask;
    a_task->Execute();
    return true;
}

class CefBrowserHost
{
public:
    static bool CreateBrowser(const CefWindowInfo&, CefRefPtr<CefClient>, const CefString&,
                              const CefBrowserSettings&, CefRefPtr<CefDictionaryValue>, void*)
    {
        ++CefCalls::browser;
        return false;
    }
    void CloseBrowser(bool) { ++CefCalls::browser; }
};
class CefBrowser
{
public:
    bool IsValid() { ++CefCalls::browser; return true; }
    int GetIdentifier() { ++CefCalls::browser; return 1; }
    CefRefPtr<CefBrowserHost> GetHost()
    {
        ++CefCalls::browser;
        return std::make_shared<CefBrowserHost>();
    }
};
