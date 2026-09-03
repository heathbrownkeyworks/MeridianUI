#pragma once

#include "PCH.h"
#include "Common/Singleton.h"
#include "Menus/FocusSessionState.h"
#include "Menus/TextInputLeaseState.h"
#include "Menus/WeakFocusOwner.h"
#include "MeridianUIAPI/ViewAPI.h"

namespace Meridian::CEF
{
    class DefaultBrowser;
}

namespace Meridian::Menus
{
    /// <summary>
    /// Single focus owner for all Meridian browsers. Owns opening/closing the
    /// transient FocusMenu and the vanilla cursor movie's visibility on focus
    /// transitions, which per-browser state cannot get right once two
    /// browsers exist.
    /// </summary>
    class FocusArbiter : public Common::Singleton<FocusArbiter>
    {
        friend class Common::Singleton<FocusArbiter>;

    protected:
        WeakFocusOwner<Meridian::CEF::DefaultBrowser> m_owner;
        FocusSessionState m_sessionState;
        TextInputLeaseState m_textInputLeaseState;

        void OnFirstClaim(bool a_pauseGame);
        void OnLastRelease();
        void QueueTextInputDesired(bool a_desired);

    public:
        bool Claim(const std::shared_ptr<Meridian::CEF::DefaultBrowser>& a_browser);
        Meridian::UI::View::FocusResult TryClaim(
            const std::shared_ptr<Meridian::CEF::DefaultBrowser>& a_browser,
            Meridian::UI::View::FocusMode a_mode);
        void Release(Meridian::CEF::DefaultBrowser* a_browser);
        bool IsOwner(const Meridian::CEF::DefaultBrowser* a_browser) const;
        bool HasOwner() const;
        bool IsOpeningKeyHeld(std::uint32_t a_scanCode) const;
        bool ConsumeOpeningKeyRelease(std::uint32_t a_scanCode);
        void OnTextInputChanged(Meridian::CEF::DefaultBrowser* a_browser, bool a_active);
        void OnFocusMenuClosed();
    };
}
