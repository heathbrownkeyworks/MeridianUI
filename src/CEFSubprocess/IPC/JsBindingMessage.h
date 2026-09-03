#pragma once

#include <string>
#include <vector>

#ifndef MERIDIAN_JS_BINDING_MESSAGE_NO_CEF
    #include <include/cef_values.h>
#endif

namespace Meridian::JS
{
    /// <summary>
    /// The authoritative shape of a JS binding IPC message.
    ///
    /// The dictionary key is ALWAYS the object name and the list values are
    /// ALWAYS function names, in both the add and the remove direction. Build
    /// messages through MakeSingleBinding() rather than assembling a
    /// CefDictionaryValue by hand, so the two directions cannot disagree.
    /// </summary>
    struct JsBindingMessage
    {
        std::string objectName;
        std::vector<std::string> funcNames;

        const std::string& DictionaryKey() const
        {
            return objectName;
        }

        const std::vector<std::string>& DictionaryValues() const
        {
            return funcNames;
        }
    };

    inline JsBindingMessage MakeSingleBinding(const char* a_objectName, const char* a_funcName)
    {
        return JsBindingMessage{
            a_objectName == nullptr ? std::string{} : std::string{a_objectName},
            {a_funcName == nullptr ? std::string{} : std::string{a_funcName}}};
    }

#ifndef MERIDIAN_JS_BINDING_MESSAGE_NO_CEF
    inline CefRefPtr<CefDictionaryValue> ToCefDictionary(const std::vector<JsBindingMessage>& a_messages)
    {
        auto dictValue = CefDictionaryValue::Create();
        for (const auto& message : a_messages)
        {
            auto listValue = CefListValue::Create();
            listValue->SetSize(message.funcNames.size());
            size_t index = 0;
            for (const auto& funcName : message.funcNames)
            {
                listValue->SetString(index++, funcName);
            }
            dictValue->SetList(message.DictionaryKey(), listValue);
        }
        return dictValue;
    }

    inline CefRefPtr<CefDictionaryValue> ToCefDictionary(const JsBindingMessage& a_message)
    {
        return ToCefDictionary(std::vector<JsBindingMessage>{a_message});
    }

    /// <summary>Returns false on a null dictionary or GetKeys failure;
    /// valid-empty returns true with an empty vector. Callers log.</summary>
    inline bool FromCefDictionary(const CefRefPtr<CefDictionaryValue>& a_dict,
                                  std::vector<JsBindingMessage>& a_out)
    {
        a_out.clear();
        if (a_dict == nullptr)
        {
            return false;
        }

        CefDictionaryValue::KeyList keys;
        if (!a_dict->GetKeys(keys))
        {
            return false;
        }

        a_out.reserve(keys.size());
        for (const auto& key : keys)
        {
            JsBindingMessage message;
            message.objectName = key.ToString();

            const auto funcList = a_dict->GetList(key);
            if (funcList != nullptr)
            {
                message.funcNames.reserve(funcList->GetSize());
                for (size_t i = 0; i < funcList->GetSize(); ++i)
                {
                    message.funcNames.push_back(funcList->GetString(i).ToString());
                }
            }

            a_out.push_back(std::move(message));
        }

        return true;
    }
#endif
}
