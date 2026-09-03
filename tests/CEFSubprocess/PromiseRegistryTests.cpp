#include "JS/PromiseRegistry.h"

#include <iostream>

namespace
{
    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestRegisterAndTake()
    {
        // Register returns distinct ids; Take retrieves exactly once.
        Meridian::JS::PromiseRegistry<int, int> reg;
        const auto id1 = reg.Register(100, 7);
        const auto id2 = reg.Register(200, 7);
        Expect(id1 != id2, "ids are distinct");
        Meridian::JS::PromiseRegistry<int, int>::Entry e{};
        Expect(reg.Take(id1, e) && e.promise == 100 && e.contextKey == 7, "take returns the entry");
        Expect(!reg.Take(id1, e), "double-take fails (settled-once)");
        Expect(reg.Size() == 1, "one entry remains");
    }

    void TestUnknownIdIsCleanMiss()
    {
        Meridian::JS::PromiseRegistry<int, int> reg;
        Meridian::JS::PromiseRegistry<int, int>::Entry e{};
        Expect(!reg.Take(42, e), "unknown id misses");
    }

    void TestDrainContextRemovesOnlyThatContext()
    {
        Meridian::JS::PromiseRegistry<int, int> reg;
        reg.Register(1, 7);
        reg.Register(2, 8);
        reg.Register(3, 7);
        const auto drained = reg.DrainContext(7);
        Expect(drained.size() == 2, "drained both context-7 entries");
        Expect(reg.Size() == 1, "context-8 entry survives");
        Meridian::JS::PromiseRegistry<int, int>::Entry e{};
        Expect(!reg.Take(1, e) || e.contextKey != 7, "drained ids are gone");
    }

    void TestDrainAbsentContextIsNoOp()
    {
        Meridian::JS::PromiseRegistry<int, int> reg;
        reg.Register(1, 7);
        Expect(reg.DrainContext(9).empty() && reg.Size() == 1, "absent context drains nothing");
    }
}

int main()
{
    TestRegisterAndTake();
    TestUnknownIdIsCleanMiss();
    TestDrainContextRemovesOnlyThatContext();
    TestDrainAbsentContextIsNoOp();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " PromiseRegistry test(s) failed\n";
        return 1;
    }

    std::cout << "All PromiseRegistry tests passed\n";
    return 0;
}
