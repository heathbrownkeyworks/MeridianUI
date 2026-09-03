#include "FocusMenu.h"

#include "FocusArbiter.h"
#include "Menus/CursorPolicy.h"

namespace
{
    std::atomic_bool g_pauseGameRequested{false};

    // Event-driven replacement for the old tick-budget retry chain: fires
    // exactly when FocusMenu actually opens, so there's nothing left to
    // poll for. Gated on HasOwner() here to skip a pointless enqueue when
    // there's plainly no owner — but that read happens on whatever thread
    // fires MenuOpenCloseEvent, and Release() (callable concurrently from
    // CEF's UI thread) can acquire the arbiter lock and enqueue
    // OnLastRelease's restore in the window between this read and the
    // AddUITask below. Left unguarded, that races to [show, hide] queue
    // order and strands the cursor invisible with no owner. HasOwner() is
    // rechecked below, inside the UI task, immediately before
    // SetVisible(false), to close that window.
    class FocusMenuOpenSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (a_event == nullptr ||
                a_event->menuName != Meridian::Menus::FocusMenu::MENU_NAME)
            {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto& focusArbiter = Meridian::Menus::FocusArbiter::GetSingleton();
            if (!a_event->opening)
            {
                // The engine has completed the asynchronous FocusMenu
                // teardown. This is the first reliable point at which its
                // input-context cleanup can no longer overwrite run mode.
                focusArbiter.OnFocusMenuClosed();
                return RE::BSEventNotifyControl::kContinue;
            }

            if (!focusArbiter.HasOwner())
            {
                return RE::BSEventNotifyControl::kContinue;
            }

            SKSE::GetTaskInterface()->AddUITask([]() {
                // Recheck immediately before acting: this UI task and any
                // OnLastRelease restore enqueued after this sink's outer
                // HasOwner() read both run as UI tasks on the same thread, in
                // queue order — if a Release() raced in between and queued
                // its restore behind this task, HasOwner() here is already
                // false and we skip, leaving the (already-queued-behind, or
                // about to run) restore as the only writer. No stale hide can
                // land after it.
                const auto cursorDecision = Meridian::Menus::CursorPolicy::Evaluate(
                    Meridian::Menus::FocusArbiter::GetSingleton().HasOwner());
                if (!cursorDecision.hideVanillaCursor)
                {
                    return;
                }

                const auto ui = RE::UI::GetSingleton();
                const auto cursorMenu = ui != nullptr ? ui->GetMenu(RE::CursorMenu::MENU_NAME) : nullptr;
                if (cursorMenu != nullptr && cursorMenu->uiMovie != nullptr)
                {
                    // Hide immediately on the focus transition. The chained
                    // CursorMenu AdvanceMovie hook maintains this state after
                    // Prisma's own per-frame visibility decision.
                    cursorMenu->uiMovie->SetVisible(false);
                }
            });
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

namespace Meridian::Menus
{
    FocusMenu::FocusMenu()
    {
        using Context = RE::UserEvents::INPUT_CONTEXT_ID;
        using MenuFlag = RE::UI_MENU_FLAGS;

        const auto scaleformManager = RE::BSScaleformManager::GetSingleton();
        if (scaleformManager == nullptr)
        {
            spdlog::error("{}: BSScaleformManager singleton is null", NameOf(FocusMenu));
            return;
        }

        const bool loaded = scaleformManager->LoadMovieEx(this, "cursormenu", []([[maybe_unused]] RE::GFxMovieDef* a_def) {});
        if (!loaded || uiMovie == nullptr)
        {
            spdlog::error("{}: failed to load cursormenu movie — focus degrades to input-capture only", NameOf(FocusMenu));
            return;
        }

        m_view = uiMovie;
        m_view->SetMouseCursorCount(1);
        m_view->SetVisible(false);

        menuFlags.set(MenuFlag::kUsesCursor,
                      MenuFlag::kModal,
                      MenuFlag::kAllowSaving,
                      MenuFlag::kAdvancesUnderPauseMenu,
                      MenuFlag::kRendersUnderPauseMenu);
        if (g_pauseGameRequested.load(std::memory_order_acquire))
        {
            menuFlags.set(MenuFlag::kPausesGame);
        }
        depthPriority = 13;
        inputContext = Context::kMenuMode;
    }

    SKSE::stl::owner<RE::IMenu*> FocusMenu::Creator()
    {
        auto menu = new FocusMenu();
        if (!menu->IsValid())
        {
            delete menu;
            return nullptr;
        }
        return menu;
    }

    void FocusMenu::AdvanceMovie([[maybe_unused]] float a_interval, [[maybe_unused]] std::uint32_t a_currentTime)
    {
    }

    RE::UI_MESSAGE_RESULTS FocusMenu::ProcessMessage(RE::UIMessage& a_message)
    {
        if (a_message.menu == FocusMenu::MENU_NAME)
        {
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return RE::UI_MESSAGE_RESULTS::kPassOn;
    }

    void FocusMenu::RegisterOpenCloseSink()
    {
        static FocusMenuOpenSink sink;
        RE::UI::GetSingleton()->AddEventSink(&sink);
    }

    void FocusMenu::Open(bool a_pauseGame)
    {
        g_pauseGameRequested.store(a_pauseGame, std::memory_order_release);
        SKSE::GetTaskInterface()->AddUITask([]() {
            const auto msgQ = RE::UIMessageQueue::GetSingleton();
            if (msgQ != nullptr)
            {
                // Deliberately unconditional — do NOT gate on a live open
                // check here. Symmetric with Close()'s unconditional kHide
                // below: from menu-open state, a Release+Claim landing in the
                // same UI-task drain queues a fresh kHide (Close) followed by
                // this kShow (Open) — both unprocessed by RE::UIMessageQueue,
                // which drains on its own per-frame pass. If Open() ran a
                // live open-check guard and observed the still-open menu
                // (its kHide not yet processed), it would skip sending
                // kShow; the queued kHide would then close the menu with no
                // matching show queued, stranding a focused browser with no
                // kMenuMode context. A
                // kShow for a menu that's already open is processed as a
                // no-op by the queue, so sending it unconditionally is always
                // safe — same inference class already accepted for Close()'s
                // unconditional kHide.
                msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            }
        });
    }

    void FocusMenu::Close()
    {
        SKSE::GetTaskInterface()->AddUITask([]() {
            const auto msgQ = RE::UIMessageQueue::GetSingleton();
            if (msgQ != nullptr)
            {
                // Deliberately unconditional — do NOT gate on a live open
                // check here. Open()'s kShow is only queued, not processed,
                // by the time its AddUITask returns (RE::UIMessageQueue
                // drains on its own per-frame pass). If Close() ran a live
                // open-check guard and lost that race, it would see "not
                // open yet" and send nothing; the in-flight kShow would then
                // open the menu later with no matching close queued,
                // stranding it open — kMenuMode input context captured, no
                // focus owner — until the next focus
                // cycle. A kHide for a menu that isn't open (yet, or at all)
                // is processed as a no-op by the queue, and a kHide queued
                // behind an in-flight kShow is processed after it, correctly
                // closing the menu. Either way this always ends up closed,
                // which is the only safe outcome for Close() to guarantee.
                msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
            }
        });
    }
}
