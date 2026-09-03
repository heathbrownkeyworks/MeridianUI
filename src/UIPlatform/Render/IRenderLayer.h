#pragma once

#include "PCH.h"
#include "RenderData.h"

namespace Meridian::Render
{
    class IRenderLayer : public virtual CefBaseRefCounted
    {
    protected:
        std::atomic_bool m_isVisible{true};
        RenderData* m_renderData = nullptr;

    public:
        virtual ~IRenderLayer() = default;

        virtual const char* GetName()
        {
            return "Unknown";
        }

        virtual void Init(RenderData* a_renderData)
        {
            ThrowIfNullptr(IRenderLayer, a_renderData);
            m_renderData = a_renderData;
        };

        virtual void SetVisible(bool a_visible)
        {
            m_isVisible.store(a_visible, std::memory_order_release);
        }

        virtual bool GetVisible()
        {
            return m_isVisible.load(std::memory_order_acquire);
        }

        virtual void BeginShutdown()
        {
            m_isVisible.store(false, std::memory_order_release);
        }

        /// Perform native GPU work before RenderHost begins the shared
        /// SpriteBatch composition pass. Implementations must preserve the
        /// immediate-context state they touch.
        virtual void Prepare() {};

        virtual void Draw() {};

        /// Called after the shared SpriteBatch has submitted this frame's
        /// draw commands. Cross-device surfaces use this point to release
        /// producer/consumer synchronization without racing GPU sampling.
        virtual void AfterDraw() {};
    };
}
