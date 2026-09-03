#pragma once

#include <memory>
#include <mutex>

namespace Meridian::Menus
{
    template <class T>
    class WeakFocusOwner
    {
    public:
        enum class TryClaimResult
        {
            claimed,
            alreadyOwner,
            busy,
            invalid,
        };

        struct ClaimResult
        {
            std::shared_ptr<T> previous;
            bool changed = false;
        };

        template <class OnFirstClaim>
        ClaimResult Claim(const std::shared_ptr<T>& a_owner, OnFirstClaim&& a_onFirstClaim)
        {
            if (a_owner == nullptr)
            {
                return {};
            }

            std::lock_guard lock(m_mutex);
            auto previous = m_owner.lock();
            if (previous == nullptr)
            {
                m_owner.reset();
                m_identity = nullptr;
            }

            if (m_identity == a_owner.get())
            {
                return {};
            }

            ClaimResult result{previous, true};
            if (m_identity == nullptr)
            {
                a_onFirstClaim();
            }

            m_owner = a_owner;
            m_identity = a_owner.get();
            return result;
        }

        template <class OnFirstClaim>
        TryClaimResult TryClaim(const std::shared_ptr<T>& a_owner, OnFirstClaim&& a_onFirstClaim)
        {
            if (a_owner == nullptr)
            {
                return TryClaimResult::invalid;
            }

            std::lock_guard lock(m_mutex);
            auto current = m_owner.lock();
            if (current == nullptr)
            {
                m_owner.reset();
                m_identity = nullptr;
            }

            if (m_identity == a_owner.get())
            {
                return TryClaimResult::alreadyOwner;
            }
            if (m_identity != nullptr)
            {
                return TryClaimResult::busy;
            }

            a_onFirstClaim();
            m_owner = a_owner;
            m_identity = a_owner.get();
            return TryClaimResult::claimed;
        }

        template <class OnLastRelease>
        std::shared_ptr<T> Release(const T* a_owner, OnLastRelease&& a_onLastRelease)
        {
            if (a_owner == nullptr)
            {
                return {};
            }

            std::lock_guard lock(m_mutex);
            if (m_identity != a_owner)
            {
                return {};
            }

            auto previous = m_owner.lock();
            m_owner.reset();
            m_identity = nullptr;
            a_onLastRelease();
            return previous;
        }

        bool IsOwner(const T* a_owner) const
        {
            std::lock_guard lock(m_mutex);
            return a_owner != nullptr && m_identity == a_owner && !m_owner.expired();
        }

        bool HasOwner() const
        {
            std::lock_guard lock(m_mutex);
            return m_identity != nullptr && !m_owner.expired();
        }

    private:
        mutable std::mutex m_mutex;
        std::weak_ptr<T> m_owner;
        const T* m_identity = nullptr;
    };
}
