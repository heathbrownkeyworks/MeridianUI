#include "JSFunctionStorage.h"
#include "IPC/JsBindingMessage.h"

namespace Meridian::JS
{
    bool JSFunctionStorage::TryBeginFunctionCallback()
    {
        std::lock_guard lock(m_callbackAdmissionMutex);
        if (m_callbackDrainState != CallbackDrainState::accepting)
        {
            return false;
        }

        ++s_callbackDepth[this];
        ++m_activeCallbackCount;
        return true;
    }

    void JSFunctionStorage::EndFunctionCallback()
    {
        const auto depthIt = s_callbackDepth.find(this);
        if (depthIt != s_callbackDepth.end())
        {
            if (--depthIt->second == 0)
            {
                s_callbackDepth.erase(depthIt);
            }
        }

        {
            std::lock_guard lock(m_callbackAdmissionMutex);
            if (m_activeCallbackCount > 0)
            {
                --m_activeCallbackCount;
            }

            if (m_callbackDrainState == CallbackDrainState::draining &&
                m_callbackMapClearedForDrain &&
                m_activeCallbackCount == 0)
            {
                m_callbackDrainState = CallbackDrainState::drained;
            }
        }
        m_callbackCondition.notify_all();
    }

    void JSFunctionStorage::ExecuteAdmittedFunctionCallback(
        const std::string& a_objectName,
        const std::string& a_funcName,
        const std::shared_ptr<std::vector<std::string>>& a_funcArgs)
    {
        if (!TryBeginFunctionCallback())
        {
            return;
        }

        try
        {
            const auto callbackData = GetFunctionCallbackData(a_objectName, a_funcName);
            if (callbackData.callback == nullptr)
            {
                spdlog::debug("{}: function callback is nullptr for {}.{}", NameOf(JSFunctionStorage), a_objectName.c_str(), a_funcName.c_str());
                EndFunctionCallback();
                return;
            }

            auto argsCharArray = Meridian::Converters::CefValueToJSONConverter::ConvertToCharArray(a_funcArgs);
            callbackData.callback(argsCharArray.data(), static_cast<int>(argsCharArray.size()));
        }
        catch (...)
        {
            EndFunctionCallback();
            throw;
        }

        EndFunctionCallback();
    }

    bool JSFunctionStorage::AddFunctionCallback(const Meridian::JS::JSFuncInfo& a_funcInfo)
    {
        if (a_funcInfo.objectName == nullptr || a_funcInfo.funcName == nullptr)
        {
            return false;
        }

        std::lock_guard lock(m_funcCallbackMapMutex);
        const auto objIt = m_funcCallbackMap.find(a_funcInfo.objectName);
        if (objIt == m_funcCallbackMap.cend())
        {
            m_funcCallbackMap.insert({a_funcInfo.objectName, {{a_funcInfo.funcName, a_funcInfo.callbackData}}});
            return true;
        }

        const auto funcIt = objIt->second.find(a_funcInfo.funcName);
        if (funcIt == objIt->second.cend())
        {
            objIt->second.insert({a_funcInfo.funcName, a_funcInfo.callbackData});
            return true;
        }

        funcIt->second = a_funcInfo.callbackData;
        return false;
    }

    bool JSFunctionStorage::RemoveFunctionCallback(const std::string& a_objectName, const std::string& a_funcName)
    {
        std::lock_guard lock(m_funcCallbackMapMutex);
        const auto objIt = m_funcCallbackMap.find(a_objectName);
        if (objIt == m_funcCallbackMap.end())
        {
            return false;
        }

        const auto funcIt = objIt->second.find(a_funcName);
        if (funcIt == objIt->second.end())
        {
            return false;
        }

        objIt->second.erase(funcIt);
        return true;
    }

    void JSFunctionStorage::ClearFunctionCallback()
    {
        std::lock_guard lock(m_funcCallbackMapMutex);
        m_funcCallbackMap.clear();
    }

    void JSFunctionStorage::DrainAndClearFunctionCallbacks()
    {
        std::unique_lock admissionLock(m_callbackAdmissionMutex);

        std::size_t currentThreadDepth = 0;
        const auto depthIt = s_callbackDepth.find(this);
        if (depthIt != s_callbackDepth.end())
        {
            currentThreadDepth = depthIt->second;
        }

        if (m_callbackDrainState == CallbackDrainState::draining)
        {
            // An admitted callback must be allowed to unwind so that the drain
            // owner can finish. A non-callback caller can safely wait for it.
            if (currentThreadDepth > 0)
            {
                return;
            }

            m_callbackCondition.wait(admissionLock, [this]() {
                return m_callbackDrainState == CallbackDrainState::drained;
            });

            std::lock_guard mapLock(m_funcCallbackMapMutex);
            m_funcCallbackMap.clear();
            return;
        }

        if (m_callbackDrainState == CallbackDrainState::drained)
        {
            std::lock_guard mapLock(m_funcCallbackMapMutex);
            m_funcCallbackMap.clear();
            return;
        }

        m_callbackDrainState = CallbackDrainState::draining;
        m_callbackMapClearedForDrain = false;

        m_callbackCondition.wait(admissionLock, [this, currentThreadDepth]() {
            return m_activeCallbackCount <= currentThreadDepth;
        });

        {
            std::lock_guard mapLock(m_funcCallbackMapMutex);
            m_funcCallbackMap.clear();
        }

        m_callbackMapClearedForDrain = true;
        if (m_activeCallbackCount == 0)
        {
            m_callbackDrainState = CallbackDrainState::drained;
        }
        admissionLock.unlock();
        m_callbackCondition.notify_all();
    }

    JSFuncCallbackData JSFunctionStorage::GetFunctionCallbackData(const std::string& a_objectName, const std::string& a_funcName)
    {
        std::lock_guard lock(m_funcCallbackMapMutex);
        const auto objIt = m_funcCallbackMap.find(a_objectName);
        if (objIt == m_funcCallbackMap.end())
        {
            return {};
        }

        const auto funcIt = objIt->second.find(a_funcName);
        if (funcIt == objIt->second.end())
        {
            return {};
        }

        return funcIt->second;
    }

    void JSFunctionStorage::ExecuteFunctionCallback(const std::string& a_objectName,
                                                    const std::string& a_funcName,
                                                    std::shared_ptr<std::vector<std::string>> a_funcArgs,
                                                    std::shared_ptr<JSFunctionStorage> a_storage)
    {
        const auto callbackData = GetFunctionCallbackData(a_objectName, a_funcName);
        if (callbackData.callback == nullptr)
        {
            spdlog::debug("{}: function callback is nullptr for {}.{}", NameOf(JSFunctionStorage), a_objectName.c_str(), a_funcName.c_str());
            return;
        }

        if (callbackData.executeInGameThread)
        {
            SKSE::GetTaskInterface()->AddTask([=]() {
                auto* callbackStorage = a_storage != nullptr ? a_storage.get() : this;
                callbackStorage->ExecuteAdmittedFunctionCallback(a_objectName, a_funcName, a_funcArgs);
            });
        }
        else
        {
            auto* callbackStorage = a_storage != nullptr ? a_storage.get() : this;
            callbackStorage->ExecuteAdmittedFunctionCallback(a_objectName, a_funcName, a_funcArgs);
        }
    }

    size_t JSFunctionStorage::GetSize()
    {
        std::lock_guard lock(m_funcCallbackMapMutex);
        size_t result = m_funcCallbackMap.size();
        for (const auto& map : m_funcCallbackMap)
        {
            result += map.second.size();
        }
        return result;
    }

    CefRefPtr<CefDictionaryValue> JSFunctionStorage::ConvertToCefDictionary()
    {
        std::lock_guard lock(m_funcCallbackMapMutex);

        std::vector<Meridian::JS::JsBindingMessage> messages;
        messages.reserve(m_funcCallbackMap.size());
        for (const auto& obj : m_funcCallbackMap)
        {
            Meridian::JS::JsBindingMessage message;
            message.objectName = obj.first;
            message.funcNames.reserve(obj.second.size());
            for (const auto& func : obj.second)
            {
                message.funcNames.push_back(func.first);
            }
            messages.push_back(std::move(message));
        }

        return Meridian::JS::ToCefDictionary(messages);
    }
}
