#include "Menus/WeakFocusOwner.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

namespace
{
    struct Owner {};
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
    Meridian::Menus::WeakFocusOwner<Owner> slot;
    int opens = 0;
    int closes = 0;
    auto first = std::make_shared<Owner>();
    auto second = std::make_shared<Owner>();

    auto claim = slot.Claim(first, [&]() { ++opens; });
    Expect(claim.changed && claim.previous == nullptr, "first claim changes owner");
    Expect(opens == 1 && slot.IsOwner(first.get()), "first claim opens and owns");
    claim = slot.Claim(first, [&]() { ++opens; });
    Expect(!claim.changed && opens == 1, "repeat claim by the live owner is idempotent");

    claim = slot.Claim(second, [&]() { ++opens; });
    Expect(claim.changed && claim.previous == first, "replacement retains previous owner safely");
    Expect(opens == 1 && slot.IsOwner(second.get()), "replacement does not reopen focus menu");

    Expect(slot.Release(first.get(), [&]() { ++closes; }) == nullptr, "stale release is ignored");
    Expect(slot.IsOwner(second.get()), "stale release preserves current owner");

    auto released = slot.Release(second.get(), [&]() { ++closes; });
    Expect(released == second && closes == 1 && !slot.HasOwner(), "current release closes and retains callback lifetime");

    auto tryResult = slot.TryClaim(second, [&]() { ++opens; });
    Expect(tryResult == Meridian::Menus::WeakFocusOwner<Owner>::TryClaimResult::claimed,
           "non-preemptive claim acquires an empty slot");
    tryResult = slot.TryClaim(second, [&]() { ++opens; });
    Expect(tryResult == Meridian::Menus::WeakFocusOwner<Owner>::TryClaimResult::alreadyOwner,
           "non-preemptive repeat claim is idempotent");
    tryResult = slot.TryClaim(first, [&]() { ++opens; });
    Expect(tryResult == Meridian::Menus::WeakFocusOwner<Owner>::TryClaimResult::busy && slot.IsOwner(second.get()),
           "non-preemptive claim never steals another live owner");
    slot.Release(second.get(), [&]() { ++closes; });

    slot.Claim(first, [&]() { ++opens; });
    claim.previous.reset();
    first.reset();
    Expect(!slot.HasOwner(), "expired weak owner is never reported live");
    tryResult = slot.TryClaim(second, [&]() { ++opens; });
    Expect(tryResult == Meridian::Menus::WeakFocusOwner<Owner>::TryClaimResult::claimed && slot.IsOwner(second.get()),
           "expired weak owner is normalized before a non-preemptive claim");
    slot.Release(second.get(), [&]() { ++closes; });

    auto current = std::make_shared<Owner>();
    slot.Claim(current, [&]() { ++opens; });
    std::atomic_bool start{false};
    std::thread reader([&]() {
        while (!start.load(std::memory_order_acquire)) {}
        for (int i = 0; i < 1000; ++i) { (void)slot.HasOwner(); }
    });
    start.store(true, std::memory_order_release);
    slot.Release(current.get(), [&]() { ++closes; });
    reader.join();
    Expect(!slot.HasOwner(), "concurrent snapshots finish without a stale owner");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " WeakFocusOwner test(s) failed\n";
        return 1;
    }
    std::cout << "All WeakFocusOwner tests passed\n";
    return 0;
}
