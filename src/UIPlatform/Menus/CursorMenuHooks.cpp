#include "CursorMenuHooks.h"

#include "Menus/FocusArbiter.h"
#include "Menus/CursorPolicy.h"

namespace Meridian::Menus
{
    void CursorMenuEx::AdvanceMovie_Hook(float a_interval, std::uint32_t a_currentTime)
    {
        // Run the previously installed hook/original first. PrismaUI installs
        // its own hook later and calls into this one after applying its
        // visibility choice; if install order changes, this call still lets
        // Prisma run before Meridian applies the final focus-scoped decision.
        s_advanceMovie(this, a_interval, a_currentTime);

        const auto cursorDecision = CursorPolicy::Evaluate(
            FocusArbiter::GetSingleton().HasOwner());
        if (cursorDecision.hideVanillaCursor && uiMovie != nullptr && uiMovie->GetVisible())
        {
            uiMovie->SetVisible(false);
        }
    }

    RE::UI_MESSAGE_RESULTS CursorMenuEx::ProcessMessage_Hook(RE::UIMessage& a_message)
    {
        if (a_message.type == RE::UI_MESSAGE_TYPE::kHide)
        {
            if (FocusArbiter::GetSingleton().HasOwner())
            {
                return RE::UI_MESSAGE_RESULTS::kIgnore;
            }

            const auto ui = RE::UI::GetSingleton();
            if (ui != nullptr && ui->IsMenuOpen(RE::Console::MENU_NAME))
            {
                return RE::UI_MESSAGE_RESULTS::kIgnore;
            }
        }

        return s_processMessage(this, a_message);
    }

    void CursorMenuEx::Install()
    {
        try
        {
            REL::Relocation<std::uintptr_t> vTable(RE::VTABLE_CursorMenu[0]);
            s_processMessage = vTable.write_vfunc(0x4, &CursorMenuEx::ProcessMessage_Hook);
            s_advanceMovie = vTable.write_vfunc(0x5, &CursorMenuEx::AdvanceMovie_Hook);
            spdlog::info("{}: CursorMenu ProcessMessage and AdvanceMovie hooks installed", NameOf(CursorMenuEx));
        }
        catch (const std::exception& e)
        {
            spdlog::error("{}: install FAILED ({}) — cursor focus arbitration unavailable", NameOf(CursorMenuEx), e.what());
        }
    }
}
