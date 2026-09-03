#include "FocusArbiter.h"

#include "CEF/DefaultBrowser.h"
#include "Menus/FocusMenu.h"
#include "Render/CursorRenderer.h"

namespace Meridian::Menus
{
    void FocusArbiter::QueueTextInputDesired(bool a_desired)
    {
        const auto generation = m_textInputLeaseState.SetDesired(a_desired);
        if (!generation.has_value())
        {
            return;
        }

        const auto taskInterface = SKSE::GetTaskInterface();
        if (taskInterface == nullptr)
        {
            spdlog::warn("FocusArbiter: could not queue text-input transition without the SKSE task interface");
            return;
        }

        taskInterface->AddTask([this, ticket = *generation]() {
            const auto controlMap = RE::ControlMap::GetSingleton();
            if (controlMap == nullptr)
            {
                spdlog::warn("FocusArbiter: could not apply text-input transition without ControlMap");
                return;
            }

            const auto transition = m_textInputLeaseState.ApplyIfCurrent(ticket);
            if (!transition.has_value())
            {
                return;
            }

            const bool acquire = *transition == TextInputLeaseState::Transition::Acquire;
            if (acquire && controlMap->GetRuntimeData().textEntryCount == -1)
            {
                // Do not remember a lease that cannot be acquired,
                // or a later blur would incorrectly decrement the sentinel.
                m_textInputLeaseState.RejectAcquire();
                spdlog::warn("FocusArbiter: engine rejected Meridian text-input lease (ControlMap count=-1)");
                return;
            }
            controlMap->AllowTextInput(acquire);
            const auto count = controlMap->GetRuntimeData().textEntryCount;
            spdlog::debug("FocusArbiter: {} Meridian text-input lease (ControlMap count={})",
                          acquire ? "acquired" : "released",
                          count);
        });
    }

    void FocusArbiter::OnFirstClaim(bool a_pauseGame)
    {
        const auto playerControls = RE::PlayerControls::GetSingleton();
        const std::optional<bool> running = playerControls != nullptr ?
            std::optional<bool>{playerControls->data.running} : std::nullopt;

        const auto inputManager = RE::BSInputDeviceManager::GetSingleton();
        const auto keyboard = inputManager != nullptr ? inputManager->GetKeyboard() : nullptr;
        const auto* keyboardState = keyboard != nullptr ? keyboard->curState : nullptr;
        const auto keyboardStateSize = keyboard != nullptr ? sizeof(keyboard->curState) : 0;
        const auto generation = m_sessionState.Begin(running, keyboardState, keyboardStateSize);
        if (running.has_value())
        {
            spdlog::debug("FocusArbiter: captured player running={} for focus generation {}",
                          *running,
                          generation);
        }

        SKSE::GetTaskInterface()->AddUITask([]() {
            Meridian::Render::CursorRenderer::GetSingleton().ResetToArrow();
        });
        Meridian::Menus::FocusMenu::Open(a_pauseGame);
    }

    void FocusArbiter::OnLastRelease()
    {
        m_sessionState.End();
        SKSE::GetTaskInterface()->AddUITask([]() {
            const auto ui = RE::UI::GetSingleton();
            const auto cursorMenu = ui != nullptr ? ui->GetMenu(RE::CursorMenu::MENU_NAME) : nullptr;
            if (cursorMenu != nullptr && cursorMenu->uiMovie != nullptr)
            {
                cursorMenu->uiMovie->SetVisible(true);
            }
        });
        Meridian::Menus::FocusMenu::Close();
    }

    bool FocusArbiter::Claim(const std::shared_ptr<Meridian::CEF::DefaultBrowser>& a_browser)
    {
        if (a_browser == nullptr)
        {
            return false;
        }

        const auto result = m_owner.Claim(a_browser, [this]() { OnFirstClaim(false); });
        if (!result.changed)
        {
            return true;
        }

        if (result.previous != nullptr)
        {
            result.previous->OnFocusRevoked();
        }
        a_browser->OnFocusGranted();
        QueueTextInputDesired(a_browser->IsTextInputActive());
        return true;
    }

    Meridian::UI::View::FocusResult FocusArbiter::TryClaim(
        const std::shared_ptr<Meridian::CEF::DefaultBrowser>& a_browser,
        Meridian::UI::View::FocusMode a_mode)
    {
        using Owner = WeakFocusOwner<Meridian::CEF::DefaultBrowser>;
        const auto result = m_owner.TryClaim(a_browser, [this, a_mode]() {
            OnFirstClaim(a_mode == Meridian::UI::View::FocusMode::PauseGame);
        });

        switch (result)
        {
        case Owner::TryClaimResult::claimed:
            QueueTextInputDesired(a_browser->IsTextInputActive());
            return Meridian::UI::View::FocusResult::Granted;
        case Owner::TryClaimResult::alreadyOwner:
            return Meridian::UI::View::FocusResult::AlreadyFocused;
        case Owner::TryClaimResult::busy:
            return Meridian::UI::View::FocusResult::Busy;
        case Owner::TryClaimResult::invalid:
        default:
            return Meridian::UI::View::FocusResult::InvalidView;
        }
    }

    void FocusArbiter::Release(Meridian::CEF::DefaultBrowser* a_browser)
    {
        auto revoked = m_owner.Release(a_browser, [this]() { OnLastRelease(); });
        if (revoked != nullptr)
        {
            QueueTextInputDesired(false);
            revoked->OnFocusRevoked();
        }
    }

    bool FocusArbiter::IsOwner(const Meridian::CEF::DefaultBrowser* a_browser) const
    {
        return m_owner.IsOwner(a_browser);
    }

    bool FocusArbiter::HasOwner() const
    {
        return m_owner.HasOwner();
    }

    bool FocusArbiter::IsOpeningKeyHeld(std::uint32_t a_scanCode) const
    {
        return m_sessionState.IsOpeningKeyHeld(a_scanCode);
    }

    bool FocusArbiter::ConsumeOpeningKeyRelease(std::uint32_t a_scanCode)
    {
        return m_sessionState.ConsumeOpeningKeyRelease(a_scanCode);
    }

    void FocusArbiter::OnTextInputChanged(Meridian::CEF::DefaultBrowser* a_browser, bool a_active)
    {
        if (IsOwner(a_browser))
        {
            QueueTextInputDesired(a_active);
        }
    }

    void FocusArbiter::OnFocusMenuClosed()
    {
        // Read owner state before session state everywhere. WeakFocusOwner
        // invokes lifecycle callbacks under its own mutex, so this ordering
        // keeps the two locks acyclic.
        if (HasOwner())
        {
            return;
        }

        const auto restore = m_sessionState.TakePendingRestore();
        if (!restore.has_value() || !restore->running.has_value())
        {
            return;
        }

        SKSE::GetTaskInterface()->AddTask([this, ticket = *restore]() {
            // A new focus claim invalidates this queued close-generation
            // restore. Never let an old menu close overwrite live input.
            if (HasOwner() || !m_sessionState.IsRestoreCurrent(ticket.generation))
            {
                return;
            }

            if (auto* playerControls = RE::PlayerControls::GetSingleton())
            {
                playerControls->data.running = *ticket.running;
                spdlog::debug("FocusArbiter: restored player running={} after focus generation {} closed",
                              *ticket.running,
                              ticket.generation);
            }
        });
    }
}
