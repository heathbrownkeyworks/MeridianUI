#include "CEF/DefaultBrowser.h"
#include <functional>
#include "include/cef_task.h"
#include "Input/ControllerTypes.h"

namespace Meridian::CEF
{
    namespace
    {
        class InputTask final : public CefTask
        {
        public:
            explicit InputTask(std::function<void()> callback)
                : m_callback(std::move(callback))
            {
            }
            void Execute() override
            {
                m_callback();
            }

        private:
            std::function<void()> m_callback;
            IMPLEMENT_REFCOUNTING(InputTask);
        };
        CefRefPtr<CefTask> MakeTask(std::function<void()> callback)
        {
            return new InputTask(std::move(callback));
        }
    }
    void DefaultBrowser::InvalidateControllerInput()
    {
        m_controllerDelivery.Invalidate();
        if (!m_shutdownStarted.load())
            ReleaseControllerPointer();
    }
    bool DefaultBrowser::SendControllerPacket(std::string json, std::uint64_t epoch)
    {
        if (m_shutdownStarted.load())
            return false;
        if (!m_controllerDelivery.TryEnqueue())
        {
            ReleaseControllerPointer(); // the overflow invalidated all queued edges
            return false;
        }
        auto self = shared_from_this();
        if (!CefPostTask(TID_UI, MakeTask([self, json = std::move(json), epoch]() {
                             self->m_controllerDelivery.Complete();
                             if (self->m_shutdownStarted.load() || self->ControllerEpoch() != epoch)
                                 return;
                             auto browser = self->m_cefClient->GetBrowser();
                             if (!browser || !self->m_cefClient->CanExposeNativeBindings())
                                 return;
                             auto frame = browser->GetMainFrame();
                             if (frame)
                                 frame->ExecuteJavaScript("window.MeridianInput && window.MeridianInput.__receive(" + json + ");", JS_EXECUTE_SCRIPT_URL, 0);
                         })))
        {
            m_controllerDelivery.Complete();
            InvalidateControllerInput();
            return false;
        }
        return true;
    }
    void DefaultBrowser::ReleaseControllerPointer()
    {
        // May run during destruction, when shared_from_this is no longer valid.
        if (m_controllerReleaseQueued.exchange(true))
            return;
        auto weak = weak_from_this();
        if (!CefPostTask(TID_UI, MakeTask([weak]() {
                             auto self = weak.lock();
                             if (!self)
                                 return;
                             self->m_controllerReleaseQueued = false;
                             if (self->m_controllerMouseButton.Apply(0) != 0)
                                 return;
                             auto browser = self->m_cefClient->GetBrowser();
                             if (browser)
                                 browser->GetHost()->SendMouseClickEvent(self->m_controllerMouseEvent, MBT_LEFT, true, 1);
                             self->m_controllerMouseEvent.modifiers = 0;
                         })))
            m_controllerReleaseQueued = false;
    }
    void DefaultBrowser::ControllerPointer(float dx, float dy, int button, std::uint64_t epoch)
    {
        // Called on the engine input thread; share coordinates with real mouse input.
        if (ControllerEpoch() != epoch || !IsBrowserFocused())
            return;
        auto cursor = RE::MenuCursor::GetSingleton();
        if (!cursor)
            return;
        auto geometry = m_geometryHolder->Get();
        if (geometry.width <= 0 || geometry.height <= 0)
            return;
        const auto position = Meridian::Input::AdvanceCursor(cursor->cursorPosX, cursor->cursorPosY, dx, dy, geometry);
        cursor->cursorPosX = position.first;
        cursor->cursorPosY = position.second;
        CefMouseEvent event{};
        Meridian::Menus::CompositorMath::ScreenToBrowser(geometry, cursor->cursorPosX, cursor->cursorPosY, event.x, event.y);
        auto self = shared_from_this();
        if (!m_controllerDelivery.TryEnqueue())
        {
            ReleaseControllerPointer();
            return;
        }
        if (!CefPostTask(TID_UI, MakeTask([self, event, button, epoch]() mutable {
                             self->m_controllerDelivery.Complete();
                             if (self->m_shutdownStarted.load() || self->ControllerEpoch() != epoch)
                                 return;
                             auto browser = self->m_cefClient->GetBrowser();
                             if (!browser)
                                 return;
                             auto host = browser->GetHost();
                             event.modifiers = self->m_controllerMouseButton.Down() ? EVENTFLAG_LEFT_MOUSE_BUTTON : 0;
                             self->m_controllerMouseEvent = event;
                             host->SendMouseMoveEvent(event, false);
                             const auto transition = self->m_controllerMouseButton.Apply(button);
                             if (transition == 1)
                             {
                                 self->m_controllerMouseEvent.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
                                 host->SendMouseClickEvent(self->m_controllerMouseEvent, MBT_LEFT, false, 1);
                             }
                             else if (transition == 0)
                             {
                                 host->SendMouseClickEvent(event, MBT_LEFT, true, 1);
                             }
                         })))
        {
            m_controllerDelivery.Complete();
            InvalidateControllerInput();
        }
    }
}
