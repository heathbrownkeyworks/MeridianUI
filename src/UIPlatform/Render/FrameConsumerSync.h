#pragma once

#include <cstdint>

namespace Meridian::Render
{
    struct ConsumerAcquirePlan
    {
        std::uint64_t acquireKey;
        bool clearsAwaitingGameAcquire;
    };

    /// A producer hands a newly copied frame to the game with key 1. After the
    /// game submits a draw, ReleaseConsumedFrame returns ownership with key 0,
    /// so every later draw of that retained frame must reacquire key 0.
    [[nodiscard]] constexpr ConsumerAcquirePlan PlanConsumerAcquire(bool a_awaitingGameAcquire) noexcept
    {
        return {
            .acquireKey = a_awaitingGameAcquire ? 1u : 0u,
            .clearsAwaitingGameAcquire = a_awaitingGameAcquire,
        };
    }
}
