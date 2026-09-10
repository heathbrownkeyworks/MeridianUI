#pragma once
#include <atomic>
#include <cstdint>

namespace Meridian::Input
{
    // Bounded deferred CEF work. Epoch changes invalidate already queued edges;
    // completing an obsolete task still returns its admission slot.
    class ControllerDeliveryState
    {
    public:
        static constexpr unsigned Capacity = 64;
        std::uint64_t Epoch() const
        {
            return m_epoch.load();
        }
        bool Matches(std::uint64_t epoch) const
        {
            return Epoch() == epoch;
        }
        void Invalidate()
        {
            m_epoch.fetch_add(1);
        }
        bool TryEnqueue()
        {
            if (m_pending.fetch_add(1) < Capacity)
                return true;
            m_pending.fetch_sub(1);
            Invalidate();
            return false;
        }
        void Complete()
        {
            m_pending.fetch_sub(1);
        }

    private:
        std::atomic_uint64_t m_epoch{1};
        std::atomic_uint32_t m_pending{0};
    };
}
