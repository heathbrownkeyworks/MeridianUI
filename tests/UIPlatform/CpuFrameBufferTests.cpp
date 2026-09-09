#include "Render/CpuFrameBuffer.h"
#include <algorithm>
#include <atomic>
#include <climits>
#include <iostream>
#include <thread>

using namespace Meridian::Render;
int failures = 0;
void Expect(bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAILED: " << message << '\n'; }
}

int main()
{
    CpuFrameBuffer frames;
    std::vector<std::uint32_t> source(16, 0xff102030);
    const PixelRect one{1, 1, 1, 1};
    Expect(frames.Submit(source.data(), 4, 4, std::span(&one, 1)), "first paint accepted");
    source.assign(16, 0xffdeadbe);
    auto first = frames.TakeUpdate();
    Expect(first && first->pixels.size() == 16 && first->pixels[0] == 0xff102030, "first paint copies the whole image and owns its pixels");
    Expect(!frames.TakeUpdate(), "unchanged content needs no new upload");
    source[5] = 0xffaabbcc;
    frames.Submit(source.data(), 4, 4, std::span(&one, 1));
    const PixelRect last{3, 3, 1, 1};
    source[15] = 0xff445566;
    frames.Submit(source.data(), 4, 4, std::span(&last, 1));
    auto combined = frames.TakeUpdate();
    Expect(combined && combined->dirty.width == 3 && combined->dirty.height == 3 &&
        combined->pixels.front() == 0xffaabbcc && combined->pixels.back() == 0xff445566 &&
        combined->pixels[1] == 0xff102030, "skipped paints coalesce without copying unchanged garbage");
    const PixelRect outside{-1, -1, 2, 2};
    source[0] = 0xffabcdef;
    frames.Submit(source.data(), 4, 4, std::span(&outside, 1));
    auto clipped = frames.TakeUpdate();
    Expect(clipped && clipped->pixels.size() == 1 && clipped->pixels[0] == 0xffabcdef, "dirty rectangles are clipped");
    const PixelRect invalid{INT_MAX, INT_MIN, INT_MAX, -1};
    frames.Submit(source.data(), 4, 4, std::span(&invalid, 1));
    Expect(!frames.TakeUpdate(), "invalid rectangle arithmetic does not overflow or dirty the image");
    Expect(!frames.Submit(nullptr, 4, 4, {}) && !frames.Submit(source.data(), 0, 4, {}) &&
        !frames.Submit(source.data(), INT_MAX, INT_MAX, {}) && !frames.Submit(source.data(), 16384, 16384, {}),
        "invalid dimensions and memory bounds rejected before reading pixels");
    frames.RequestFullUpload();
    auto retry = frames.TakeUpdate();
    Expect(retry && retry->pixels.size() == 16 && retry->pixels[5] == 0xffaabbcc, "failed upload can retry a complete retained image");
    std::uint32_t alpha[] = {0x80402010, 0x00112233, 0xff112233};
    frames.Submit(alpha, 3, 1, std::span(&one, 1));
    auto resized = frames.TakeUpdate();
    Expect(resized && resized->pixels.size() == 3 && resized->pixels[0] == 0x80804020 &&
        resized->pixels[1] == 0 && resized->pixels[2] == 0xff112233, "resize forces full paint and converts alpha for the compositor");
    Expect(resized && first && resized->epoch != first->epoch, "resize invalidates the old texture epoch");
    const auto epoch = resized->epoch;
    frames.Reset();
    Expect(!frames.IsCurrent(epoch) && !frames.TakeUpdate(), "popup reset invalidates retained GPU content");
    frames.Submit(alpha, 3, 1, {});
    frames.Stop();
    frames.Reset();
    Expect(!frames.Submit(alpha, 3, 1, {}) && !frames.TakeUpdate(), "stop is terminal even after reset");

    CpuFrameBuffer concurrent;
    std::atomic_bool done{false};
    std::thread producer([&] {
        for (std::uint32_t i = 0; i < 2000; ++i)
        {
            const int width = 8 + (i % 2);
            std::vector<std::uint32_t> paint(width * 8, 0xff000000 | i);
            concurrent.Submit(paint.data(), width, 8, {});
        }
        done = true;
    });
    do
    {
        if (auto update = concurrent.TakeUpdate())
            Expect(update->pixels.size() == std::size_t(update->width * update->height) &&
                std::all_of(update->pixels.begin(), update->pixels.end(), [&](auto pixel) { return pixel == update->pixels.front(); }),
                "producer/consumer resize snapshots remain coherent");
    } while (!done.load());
    producer.join();
    std::cout << "CpuFrameBuffer failures: " << failures << '\n';
    return failures ? 1 : 0;
}
