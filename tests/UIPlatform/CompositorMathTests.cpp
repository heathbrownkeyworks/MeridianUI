#include "Menus/CompositorMath.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>

namespace
{
    using namespace Meridian::Menus::CompositorMath;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestLogicalSize()
    {
        const LayerGeometry g{100, 50, 800, 600, 0.5f};
        Expect(LogicalWidth(g) == 400, "logical width is rect width times scale");
        Expect(LogicalHeight(g) == 300, "logical height is rect height times scale");

        const LayerGeometry unit{0, 0, 1920, 1080, 1.0f};
        Expect(LogicalWidth(unit) == 1920 && LogicalHeight(unit) == 1080, "scale 1.0 is identity");

        const LayerGeometry tiny{0, 0, 1, 1, 0.1f};
        Expect(LogicalWidth(tiny) >= 1 && LogicalHeight(tiny) >= 1, "logical size never reaches zero");
    }

    void TestDrawOrdering()
    {
        // Lower z draws first; equal z breaks ties by creation order.
        Expect(DrawsBefore({0, 5}, {1, 1}), "lower z draws first regardless of creation order");
        Expect(DrawsBefore({2, 1}, {2, 9}), "equal z breaks ties by earlier creation");
        Expect(!DrawsBefore({2, 9}, {2, 1}), "tie-break is asymmetric");
        Expect(!DrawsBefore({3, 4}, {3, 4}), "irreflexive (strict weak ordering)");

        // A full sort must be deterministic for a shuffled input.
        std::vector<LayerKey> keys{{1, 3}, {0, 7}, {1, 1}, {0, 2}, {-5, 9}};
        std::stable_sort(keys.begin(), keys.end(), [](const LayerKey& a, const LayerKey& b) { return DrawsBefore(a, b); });
        Expect(keys[0].zOrder == -5, "most negative z draws first");
        Expect(keys[1].zOrder == 0 && keys[1].creationSeq == 2, "z 0 ties by creationSeq 2 first");
        Expect(keys[2].zOrder == 0 && keys[2].creationSeq == 7, "z 0 creationSeq 7 second");
        Expect(keys[3].zOrder == 1 && keys[3].creationSeq == 1, "z 1 ties by creationSeq");
    }

    void TestHitTest()
    {
        const LayerGeometry g{100, 200, 300, 150, 1.0f};
        Expect(HitTest(g, 100, 200), "top-left corner is inside (closed lower bound)");
        Expect(HitTest(g, 399, 349), "bottom-right interior point is inside");
        Expect(!HitTest(g, 400, 349), "right edge is outside (open upper bound)");
        Expect(!HitTest(g, 399, 350), "bottom edge is outside (open upper bound)");
        Expect(!HitTest(g, 99, 250), "left of rect is outside");
        Expect(!HitTest(g, 150, 199), "above rect is outside");
    }

    void TestScreenToBrowser()
    {
        // Scale 1: pure translation.
        const LayerGeometry g1{100, 200, 300, 150, 1.0f};
        int x = 0, y = 0;
        ScreenToBrowser(g1, 100.0f, 200.0f, x, y);
        Expect(x == 0 && y == 0, "origin maps to local 0,0 at scale 1");
        ScreenToBrowser(g1, 250.0f, 275.0f, x, y);
        Expect(x == 150 && y == 75, "interior point translates at scale 1");

        // Scale 0.5: the page is HALF the rect size, so local = offset * 0.5.
        // A point at the rect's right edge maps to the logical width.
        const LayerGeometry g2{0, 0, 800, 600, 0.5f};
        ScreenToBrowser(g2, 800.0f, 600.0f, x, y);
        Expect(x == 400 && y == 300, "rect far corner maps to logical size at scale 0.5");
        ScreenToBrowser(g2, 400.0f, 300.0f, x, y);
        Expect(x == 200 && y == 150, "rect midpoint maps to logical midpoint at scale 0.5");

        // Scale 2: page is larger than the rect.
        const LayerGeometry g3{10, 10, 100, 100, 2.0f};
        ScreenToBrowser(g3, 110.0f, 110.0f, x, y);
        Expect(x == 200 && y == 200, "rect far corner maps to logical size at scale 2");
    }

    void TestRescaleForResolution()
    {
        // 1920x1080 -> 2560x1440 keeps proportions.
        const LayerGeometry g{192, 108, 960, 540, 0.75f};
        const auto r = RescaleForResolution(g, 1920, 1080, 2560, 1440);
        Expect(r.x == 256 && r.y == 144, "origin rescales proportionally");
        Expect(r.width == 1280 && r.height == 720, "size rescales proportionally");
        Expect(r.resolutionScale == 0.75f, "resolutionScale is preserved");

        // Fullscreen stays fullscreen.
        const LayerGeometry fs{0, 0, 1920, 1080, 1.0f};
        const auto rfs = RescaleForResolution(fs, 1920, 1080, 1280, 720);
        Expect(rfs.x == 0 && rfs.y == 0 && rfs.width == 1280 && rfs.height == 720, "fullscreen tracks the new resolution");

        // Degenerate old size must not divide by zero.
        const auto rz = RescaleForResolution(g, 0, 0, 1920, 1080);
        Expect(rz.width == g.width && rz.height == g.height, "zero old resolution returns geometry unchanged");
    }

    void TestBrowserRectToScreen()
    {
        // Identity: scale 1.0, origin 0 — popup rect passes through unchanged.
        const LayerGeometry identity{0, 0, 800, 600, 1.0f};
        ScreenRect r = BrowserRectToScreen(identity, 10, 20, 30, 40);
        Expect(r.left == 10 && r.top == 20 && r.right == 40 && r.bottom == 60,
               "identity geometry passes the rect through unchanged");

        // Scale 0.5: inverse scale is 2x, so local coordinates double.
        const LayerGeometry half{0, 0, 800, 600, 0.5f};
        r = BrowserRectToScreen(half, 10, 20, 30, 40);
        Expect(r.left == 20 && r.top == 40 && r.right == 80 && r.bottom == 120,
               "scale 0.5 doubles coordinates via the inverse scale");

        // Offset origin: geometry origin is added to the scaled local coordinates.
        const LayerGeometry offset{100, 200, 800, 600, 1.0f};
        r = BrowserRectToScreen(offset, 10, 20, 30, 40);
        Expect(r.left == 110 && r.top == 220 && r.right == 140 && r.bottom == 260,
               "offset origin adds to the local coordinates");

        // Round-trip with ScreenToBrowser on the rect's corner point, within 1px.
        const LayerGeometry g{50, 75, 800, 600, 0.75f};
        const int localX = 123;
        const int localY = 456;
        r = BrowserRectToScreen(g, localX, localY, 10, 10);
        int backX = 0, backY = 0;
        ScreenToBrowser(g, static_cast<float>(r.left), static_cast<float>(r.top), backX, backY);
        Expect(std::abs(backX - localX) <= 1 && std::abs(backY - localY) <= 1,
               "round-trip through ScreenToBrowser recovers the local corner within 1px");

        // Degenerate scale 0 is treated as 1.0 (no divide-by-zero, identity inverse).
        const LayerGeometry zeroScale{0, 0, 800, 600, 0.0f};
        r = BrowserRectToScreen(zeroScale, 10, 20, 30, 40);
        Expect(r.left == 10 && r.top == 20 && r.right == 40 && r.bottom == 60,
               "degenerate scale 0 is treated as 1.0");
    }

    void TestRingStateMachine()
    {
        // The writer must never pick the published slot or the reader-held slot.
        RingState s{0, -1, -1};
        Expect(NextWriteSlot(s, 3) == 0 || s.writeSlot == 0, "initial write slot is 0");

        Publish(s, 3);
        Expect(s.publishedSlot == 0, "first publish exposes slot 0");
        Expect(s.writeSlot != s.publishedSlot, "writer moved off the published slot");

        // Reader latches the published slot; writer must now avoid BOTH.
        s.readerSlot = s.publishedSlot;
        Publish(s, 3);
        Expect(s.publishedSlot != s.readerSlot, "second publish is a different slot than reader holds");
        Expect(s.writeSlot != s.publishedSlot && s.writeSlot != s.readerSlot,
               "writer avoids both published and reader-held slots");

        // Long alternation never collides.
        RingState t{0, -1, -1};
        for (int i = 0; i < 100; ++i)
        {
            const int writing = t.writeSlot;
            Expect(writing != t.publishedSlot && writing != t.readerSlot, "write slot free on every iteration");
            Publish(t, 3);
            t.readerSlot = t.publishedSlot; // reader always latches the newest
        }
    }
}

int main()
{
    TestLogicalSize();
    TestDrawOrdering();
    TestHitTest();
    TestScreenToBrowser();
    TestBrowserRectToScreen();
    TestRescaleForResolution();
    TestRingStateMachine();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " CompositorMath test(s) failed\n";
        return 1;
    }

    std::cout << "All CompositorMath tests passed\n";
    return 0;
}
