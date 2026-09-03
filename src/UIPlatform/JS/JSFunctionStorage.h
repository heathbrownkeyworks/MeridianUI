#pragma once

#include "PCH.h"
#include "Converters/CefValueToJSONConverter.h"

namespace Meridian::JS
{
    class JSFunctionStorage
    {
      protected:
        enum class CallbackDrainState
        {
            accepting,
            draining,
            drained
        };

        std::recursive_mutex m_funcCallbackMapMutex;
        std::mutex m_callbackAdmissionMutex;
        std::condition_variable m_callbackCondition;
        CallbackDrainState m_callbackDrainState = CallbackDrainState::accepting;
        bool m_callbackMapClearedForDrain = false;
        std::size_t m_activeCallbackCount = 0;
        static inline thread_local std::unordered_map<const JSFunctionStorage*, std::size_t> s_callbackDepth;
        std::unordered_map<std::string, std::unordered_map<std::string, Meridian::JS::JSFuncCallbackData>> m_funcCallbackMap;

        bool TryBeginFunctionCallback();
        void EndFunctionCallback();
        void ExecuteAdmittedFunctionCallback(const std::string& a_objectName,
                                             const std::string& a_funcName,
                                             const std::shared_ptr<std::vector<std::string>>& a_funcArgs);

      public:
        sigslot::signal<> OnQueueItemAdded;

        // Returns true if the function is new, otherwise false (replaced)
        virtual bool AddFunctionCallback(const Meridian::JS::JSFuncInfo& a_funcInfo);
        // Returns true if the function found and removed, otherwise false
        virtual bool RemoveFunctionCallback(const std::string& a_objectName, const std::string& a_funcName);
        virtual void ClearFunctionCallback();
        virtual void DrainAndClearFunctionCallbacks();
        virtual JSFuncCallbackData GetFunctionCallbackData(const std::string& a_objectName, const std::string& a_funcName);
        virtual void ExecuteFunctionCallback(const std::string& a_objectName,
                                             const std::string& a_funcName,
                                             std::shared_ptr<std::vector<std::string>> a_funcArgs,
                                             std::shared_ptr<JSFunctionStorage> a_storage = nullptr);
        size_t GetSize();

        virtual CefRefPtr<CefDictionaryValue> ConvertToCefDictionary();
    };
}
