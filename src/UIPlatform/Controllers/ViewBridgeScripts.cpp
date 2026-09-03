#include "Controllers/ViewBridgeScripts.h"

#include <nlohmann/json.hpp>

namespace Meridian::Controllers::ViewBridgeScripts
{
    std::string BuildBootstrap(std::string_view a_token)
    {
        const auto token = nlohmann::json(a_token).dump();
        std::string script = R"JS((function(token){
            if (window.__meridianViewBootstrapStarted === true) return;
            Object.defineProperty(window, '__meridianViewBootstrapStarted', {
                configurable: true,
                value: true
            });

            let initialized = false;
            function initialize() {
                if (initialized) return;

                const nativeObject = window.MeridianViewNative;
                if (!nativeObject || typeof nativeObject.dispatch !== 'function') {
                    window.setTimeout(initialize, 16);
                    return;
                }

                initialized = true;
                const dispatch = nativeObject.dispatch.bind(nativeObject);
                const bound = Object.create(null);
                Object.defineProperty(window, '__meridianViewBoundListeners', {
                    configurable: true,
                    value: bound
                });

                const bind = function(name) {
                    if (typeof name !== 'string' || name.length === 0 || bound[name] === true) return;
                    try {
                        Object.defineProperty(window, name, {
                            configurable: true,
                            value: function(data) {
                                let payload = data == null ? '' :
                                    (typeof data === 'string' ? data : JSON.stringify(data));
                                if (typeof payload !== 'string') payload = '';
                                dispatch(token, name, payload);
                            }
                        });
                        bound[name] = true;
                    } catch (_) {
                        // A conflicting non-configurable page global must not block other listeners.
                    }
                };

                Object.defineProperty(window, '__meridianViewBind', {
                    configurable: true,
                    value: bind
                });

                const pending = Array.isArray(window.__meridianViewPendingListeners) ?
                    window.__meridianViewPendingListeners : [];
                pending.splice(0).forEach(bind);

                const textEntryTypes = Object.freeze({
                    text: true,
                    search: true,
                    number: true,
                    password: true,
                    email: true,
                    tel: true,
                    url: true,
                    date: true,
                    'datetime-local': true,
                    month: true,
                    time: true,
                    week: true
                });
                const isTextEntry = function(element) {
                    if (!element) return false;
                    if (element.isContentEditable === true) return true;

                    const tagName = typeof element.tagName === 'string' ?
                        element.tagName.toUpperCase() : '';
                    if (tagName === 'TEXTAREA') return true;
                    if (tagName !== 'INPUT') return false;

                    const type = typeof element.type === 'string' && element.type.length > 0 ?
                        element.type.toLowerCase() : 'text';
                    return textEntryTypes[type] === true;
                };

                let lastTextInputState = null;
                const reportTextInput = function() {
                    const active = isTextEntry(document.activeElement);
                    if (active === lastTextInputState) return;
                    lastTextInputState = active;
                    dispatch(token, '__meridian_text_input', active ? '1' : '0');
                };

                document.addEventListener('focusin', reportTextInput, true);
                document.addEventListener('focusout', function() {
                    // focusout fires before document.activeElement settles. A
                    // zero-delay check also avoids a false release/acquire pair
                    // when focus moves directly between two editable controls.
                    window.setTimeout(reportTextInput, 0);
                }, true);
                reportTextInput();
                dispatch(token, '__meridian_dom_ready', '');
            }

            initialize();
        }))JS";
        script += '(';
        script += token;
        script += ");";
        return script;
    }

    std::string BuildListener(std::string_view a_listenerName)
    {
        const auto listenerName = nlohmann::json(a_listenerName).dump();
        std::string script = R"JS((function(name){
            if (typeof window.__meridianViewBind === 'function') {
                window.__meridianViewBind(name);
                return;
            }

            let pending = window.__meridianViewPendingListeners;
            if (!Array.isArray(pending)) {
                pending = [];
                Object.defineProperty(window, '__meridianViewPendingListeners', {
                    configurable: true,
                    writable: true,
                    value: pending
                });
            }
            if (pending.indexOf(name) === -1) pending.push(name);
        }))JS";
        script += '(';
        script += listenerName;
        script += ");";
        return script;
    }
}
