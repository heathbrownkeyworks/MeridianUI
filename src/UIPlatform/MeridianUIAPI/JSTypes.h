// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <utility>

namespace Meridian::JS
{
    using JSFuncCallback = void (*)(const char** a_args, int a_argsCount);

    struct JSFuncCallbackData
    {
        JSFuncCallback callback = nullptr;
        bool executeInGameThread = true;
        bool isEventFunction = false;
    };

    struct JSFuncInfo
    {
        const char* objectName = nullptr;
        const char* funcName = nullptr;
        JSFuncCallbackData callbackData;
    };

    struct JSFuncInfoString : public JSFuncInfo
    {
        JSFuncInfoString() = default;
        JSFuncInfoString(const char* a_objectName, const char* a_funcName)
            : objectNameString(a_objectName),
              funcNameString(a_funcName)
        {
            objectName = objectNameString.c_str();
            funcName = funcNameString.c_str();
        }
        JSFuncInfoString(const JSFuncInfo& a_info)
            : objectNameString(a_info.objectName),
              funcNameString(a_info.funcName)
        {
            objectName = objectNameString.c_str();
            funcName = funcNameString.c_str();
            callbackData = a_info.callbackData;
        }
        JSFuncInfoString(JSFuncInfoString& a_other)
        {
            objectNameString = a_other.objectNameString;
            funcNameString = a_other.funcNameString;
            objectName = objectNameString.c_str();
            funcName = funcNameString.c_str();
            callbackData = a_other.callbackData;
        }
        JSFuncInfoString(JSFuncInfoString&& a_other) noexcept
        {
            objectNameString = std::move(a_other.objectNameString);
            funcNameString = std::move(a_other.funcNameString);
            objectName = objectNameString.c_str();
            funcName = funcNameString.c_str();
            callbackData = a_other.callbackData;

            a_other.objectName = nullptr;
            a_other.funcName = nullptr;
        }

        std::string objectNameString;
        std::string funcNameString;
    };

    /// <summary>
    /// The platform retains every resolver instance for the process lifetime
    /// (see PromiseRouter's memory-model comment) rather than freeing it on
    /// settle, so the "callable from any thread, at any time" contract below
    /// can never race a delete. A virtual destructor is required for that
    /// retention to be well-defined when the platform owns
    /// instances through this base pointer.
    /// </summary>
    class IJSPromiseResolver
    {
    public:
        virtual ~IJSPromiseResolver() = default;

        /// <summary>Settle the JS promise with a JSON payload (object, array,
        /// string, number, bool, or null — parsed on the JS side). One-shot:
        /// the first Resolve or Reject wins; later calls are logged no-ops.
        /// Callable from any thread, at any time, including after the browser
        /// has closed (then it is a safe no-op). A payload that is not valid
        /// JSON rejects the promise instead; nullptr resolves with null.</summary>
        virtual void __cdecl Resolve(const char* a_jsonPayload) = 0;
        /// <summary>Settle the JS promise as rejected with an Error whose
        /// message is a_errorMessage. Same one-shot/thread/lifetime rules.</summary>
        virtual void __cdecl Reject(const char* a_errorMessage) = 0;
    };

    using JSPromiseCallback = void (*)(const char** a_args, int a_argsCount,
                                       IJSPromiseResolver* a_resolver);

    struct JSPromiseFuncInfo
    {
        const char* objectName = nullptr;
        const char* funcName = nullptr;
        JSPromiseCallback callback = nullptr;
        bool executeInGameThread = true;
    };
}
