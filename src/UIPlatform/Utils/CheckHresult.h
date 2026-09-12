#pragma once

#include <comdef.h>
#include <windows.h>

// _com_error::ErrorMessage() returns a wide (LPCTSTR) string
// Convert the message to a narrow (UTF-8) std::string for formatting.
inline std::string NarrowErrorMessage(LPCTSTR wideMsg)
{
    if (wideMsg == nullptr)
    {
        return {};
    }

    const int wideLen = lstrlenW(wideMsg);
    if (wideLen == 0)
    {
        return {};
    }

    const int byteLen = WideCharToMultiByte(CP_UTF8, 0, wideMsg, wideLen, nullptr, 0, nullptr, nullptr);
    if (byteLen <= 0)
    {
        return {};
    }

    std::string narrow(static_cast<size_t>(byteLen), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideMsg, wideLen, narrow.data(), byteLen, nullptr, nullptr);
    return narrow;
}

inline std::string CheckHresultMessage(HRESULT hr, const std::string& userMsg)
{
    if (!FAILED(hr))
    {
        return "";
    }

    _com_error err(hr);
    const std::string errMsg = NarrowErrorMessage(err.ErrorMessage());
    return fmt::format("{}: unexpected HRESULT {:#X}: {}", userMsg, static_cast<unsigned long>(hr), errMsg);
}

inline void CheckHresultThrow(HRESULT hr, const std::string& userMsg)
{
    if (auto msg = CheckHresultMessage(hr, userMsg); !msg.empty())
    {
        throw std::runtime_error(std::move(msg));
    }
}

#define CHECK_HRESULT_LOG_AND_RETURN(hr, userMsg)                      \
    do                                                                 \
    {                                                                  \
        if (auto msg = CheckHresultMessage(hr, userMsg); !msg.empty()) \
        {                                                              \
            LOG_ERROR("{}", msg);                                  \
            return;                                                    \
        }                                                              \
    } while (0)

#define FAST_CHECK_HRESULT_LOG_AND_RETURN(hr, userMsg)             \
    do                                                             \
    {                                                              \
        if (FAILED(hr))                                            \
        {                                                          \
            LOG_ERROR("{}", CheckHresultMessage(hr, userMsg)); \
            return;                                                \
        }                                                          \
    } while (0)
