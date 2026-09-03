#pragma once

#include "PCH.h"
#include "Render/CursorRasterizer.h"
#include "Render/CursorTextureCache.h"
#include "Render/RenderData.h"

namespace Meridian::Render
{
    /// <summary>
    /// Draws Meridian's cursor at present time while a browser is focused
    /// (the platform UI is always-on-top, so the vanilla cursor would be
    /// covered). The current HCURSOR comes from CEF OnCursorChange and is
    /// rasterized (see CursorRasterizer.h) via DrawIconEx. Only the render
    /// thread touches the cache; NotifyCursor is a single atomic store.
    /// </summary>
    class CursorRenderer
    {
    public:
        static CursorRenderer& GetSingleton();

        void NotifyCursor(HCURSOR a_cursor);
        void ResetToArrow();
        void Draw(RenderData& a_renderData);  // render thread, inside SpriteBatch Begin/End

    protected:
        std::atomic<HCURSOR> m_current{nullptr};
        std::atomic<std::uint64_t> m_revision{0};
        std::uint64_t m_appliedRevision = 0;  // render thread only
        CursorTextureCache<RasterizedCursor> m_cache;  // render-thread only
    };
}
