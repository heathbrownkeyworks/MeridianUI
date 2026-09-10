#include "Input/ControllerDeliveryState.h"
#include "Input/ControllerCursorState.h"
#include "Input/ControllerState.h"
#include <vector>
using namespace Meridian::Input;
int main()
{
    ControllerDeliveryState delivery;
    std::vector<std::uint64_t> queued;
    for (unsigned i = 0; i < ControllerDeliveryState::Capacity; ++i)
    {
        if (!delivery.TryEnqueue())
            return 1;
        queued.push_back(delivery.Epoch());
    }
    if (delivery.TryEnqueue())
        return 2;
    unsigned delivered = 0;
    for (auto ticket : queued)
    {
        delivery.Complete();
        if (delivery.Matches(ticket))
            ++delivered;
    }
    if (delivered != 0 || !delivery.TryEnqueue())
        return 3;
    const auto beforeClose = delivery.Epoch();
    delivery.Invalidate(); // close/reload/renderer loss must drop a queued accept
    delivery.Complete();
    if (delivery.Matches(beforeClose) || !delivery.TryEnqueue())
        return 4;
    const auto reopened = delivery.Epoch();
    delivery.Complete();
    if (!delivery.Matches(reopened))
        return 5;

    // Recording host: cancel a live synthetic drag, then reject its queued move
    // and release. Reconnect permits a fresh press only after a neutral sample.
    PointerButtonState pointer;
    std::vector<int> hostEdges;
    auto send = [&](int value) {auto edge=pointer.Apply(value);if(edge!=-1)hostEdges.push_back(edge); };
    ControllerState state;
    state.SetOwner(1);
    if (state.Button(Control::South, 1).phase == Phase::Press)
        send(1);
    const auto dragEpoch = delivery.Epoch();
    state.ResetDevice();
    state.SetOwner(0);
    delivery.Invalidate();
    send(0);
    if (delivery.Matches(dragEpoch))
        send(0);
    send(0); // another cancellation is idempotent
    if (hostEdges != std::vector<int>{1, 0} || pointer.Down())
        return 6;
    state.Seed(Control::South, 1);
    state.SetOwner(1);
    if (state.Button(Control::South, 1).phase != Phase::None)
        return 7;
    state.Button(Control::South, 0);
    if (state.Button(Control::South, 1).phase != Phase::Press)
        return 8;
    return 0;
}
