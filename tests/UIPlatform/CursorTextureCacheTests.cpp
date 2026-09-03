#include "Render/CursorTextureCache.h"

#include <iostream>

namespace
{
    using namespace Meridian::Render;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestHitReturnsStoredValue()
    {
        CursorTextureCache<int> cache;
        cache.Put(0x10, 7);
        const int* hit = cache.Get(0x10);
        Expect(hit != nullptr && *hit == 7, "stored value is retrievable");
    }

    void TestMissReturnsNullptr()
    {
        CursorTextureCache<int> cache;
        Expect(cache.Get(0x99) == nullptr, "unknown key misses");
    }

    void TestCapacityEvictsLeastRecentlyUsed()
    {
        CursorTextureCache<int> cache;
        for (int i = 0; i < 8; ++i) cache.Put(0x100 + i, i);
        (void)cache.Get(0x100);              // touch key 0x100 — now 0x101 is LRU
        cache.Put(0x200, 42);                // evicts 0x101
        Expect(cache.Size() == 8, "size stays at capacity");
        Expect(cache.Get(0x101) == nullptr, "LRU entry evicted");
        Expect(cache.Get(0x100) != nullptr, "recently-used entry survives");
        Expect(cache.Get(0x200) != nullptr, "new entry present");
    }

    void TestRePutReplacesWithoutEviction()
    {
        CursorTextureCache<int> cache;
        cache.Put(0x1, 1);
        cache.Put(0x1, 2);
        Expect(cache.Size() == 1, "re-put does not grow");
        Expect(*cache.Get(0x1) == 2, "re-put replaces value");
    }

    void TestEraseInvalidatesReusedHandle()
    {
        CursorTextureCache<int> cache;
        cache.Put(0x1, 1);
        Expect(cache.Erase(0x1), "existing cursor handle is erased");
        Expect(cache.Get(0x1) == nullptr, "erased cursor handle cannot return stale pixels");
        Expect(!cache.Erase(0x1), "erasing an absent cursor handle is harmless");
    }
}

int main()
{
    TestHitReturnsStoredValue();
    TestMissReturnsNullptr();
    TestCapacityEvictsLeastRecentlyUsed();
    TestRePutReplacesWithoutEviction();
    TestEraseInvalidatesReusedHandle();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " CursorTextureCache test(s) failed\n";
        return 1;
    }

    std::cout << "All CursorTextureCache tests passed\n";
    return 0;
}
