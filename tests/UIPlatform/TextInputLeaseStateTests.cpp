#include "Menus/TextInputLeaseState.h"

#include <iostream>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    using State = Meridian::Menus::TextInputLeaseState;

    State state;
    Expect(!state.SetDesired(false).has_value(), "initial inactive state is a no-op");

    const auto acquire = state.SetDesired(true);
    Expect(acquire.has_value() && state.IsDesired(), "editable focus requests a lease");
    const auto acquireTransition = state.ApplyIfCurrent(*acquire);
    Expect(acquireTransition == State::Transition::Acquire && state.IsApplied(),
           "current editable-focus request acquires exactly one lease");
    Expect(!state.SetDesired(true).has_value(), "duplicate focus does not acquire twice");

    const auto release = state.SetDesired(false);
    Expect(release.has_value() && !state.IsDesired(), "focus loss requests release");
    const auto releaseTransition = state.ApplyIfCurrent(*release);
    Expect(releaseTransition == State::Transition::Release && !state.IsApplied(),
           "current focus-loss request releases the acquired lease");
    Expect(!state.SetDesired(false).has_value(), "duplicate blur does not release twice");

    State rapidBlur;
    const auto staleAcquire = rapidBlur.SetDesired(true);
    const auto newestBlur = rapidBlur.SetDesired(false);
    Expect(staleAcquire.has_value() && newestBlur.has_value(), "rapid focus and blur produce ordered generations");
    Expect(!rapidBlur.ApplyIfCurrent(*staleAcquire).has_value(), "stale acquire is ignored");
    Expect(!rapidBlur.ApplyIfCurrent(*newestBlur).has_value(),
           "blur without an applied acquire does not release another owner's lease");

    State rapidRefocus;
    const auto firstAcquire = rapidRefocus.SetDesired(true);
    Expect(rapidRefocus.ApplyIfCurrent(*firstAcquire) == State::Transition::Acquire,
           "refocus fixture starts with an applied lease");
    const auto staleRelease = rapidRefocus.SetDesired(false);
    const auto newestRefocus = rapidRefocus.SetDesired(true);
    Expect(!rapidRefocus.ApplyIfCurrent(*staleRelease).has_value(), "stale release is ignored");
    Expect(!rapidRefocus.ApplyIfCurrent(*newestRefocus).has_value() && rapidRefocus.IsApplied(),
           "rapid refocus keeps the existing lease without incrementing it");

    State rejectedAcquire;
    const auto rejectedTicket = rejectedAcquire.SetDesired(true);
    Expect(rejectedAcquire.ApplyIfCurrent(*rejectedTicket) == State::Transition::Acquire,
           "rejected-acquire fixture tentatively applies a lease");
    rejectedAcquire.RejectAcquire();
    Expect(rejectedAcquire.IsDesired() && !rejectedAcquire.IsApplied(),
           "an engine-rejected acquire is not remembered as an owned lease");
    const auto rejectedBlur = rejectedAcquire.SetDesired(false);
    Expect(!rejectedAcquire.ApplyIfCurrent(*rejectedBlur).has_value(),
           "blur after a rejected acquire does not decrement the engine sentinel");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " text-input lease test(s) failed\n";
        return 1;
    }
    std::cout << "All text-input lease tests passed\n";
    return 0;
}
