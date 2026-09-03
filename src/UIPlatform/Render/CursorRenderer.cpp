#include "CursorRenderer.h"

namespace Meridian::Render
{
    CursorRenderer& CursorRenderer::GetSingleton()
    {
        static CursorRenderer instance;
        return instance;
    }

    void CursorRenderer::NotifyCursor(HCURSOR a_cursor)
    {
        m_current.store(a_cursor, std::memory_order_release);
        m_revision.fetch_add(1, std::memory_order_release);
    }

    void CursorRenderer::ResetToArrow()
    {
        // LoadCursor (not LoadCursorW): the project doesn't define UNICODE,
        // so IDC_ARROW is the ANSI (LPSTR) resource id form; LoadCursorW
        // demands LPCWSTR and the two don't implicitly convert.
        m_current.store(::LoadCursor(nullptr, IDC_ARROW), std::memory_order_release);
        m_revision.fetch_add(1, std::memory_order_release);
    }

    void CursorRenderer::Draw(RenderData& a_renderData)
    {
        const HCURSOR cursor = m_current.load(std::memory_order_acquire);
        if (cursor == nullptr)
        {
            return;
        }

        const auto key = reinterpret_cast<std::uintptr_t>(cursor);
        const auto revision = m_revision.load(std::memory_order_acquire);
        if (revision != m_appliedRevision)
        {
            // CEF/Windows may reuse an HCURSOR value for different custom
            // pixels. Invalidate that key whenever CEF reports a change.
            m_cache.Erase(key);
            m_appliedRevision = revision;
        }
        RasterizedCursor* entry = m_cache.Get(key);
        if (entry == nullptr)
        {
            RasterizedCursor fresh;
            if (!RasterizeCursor(a_renderData.device, cursor, fresh))
            {
                return;
            }
            m_cache.Put(key, std::move(fresh));
            entry = m_cache.Get(key);
        }
        if (entry == nullptr || entry->srv == nullptr)
        {
            return;
        }

        const auto menuCursor = RE::MenuCursor::GetSingleton();
        if (menuCursor == nullptr)
        {
            return;
        }
        const float x = menuCursor->cursorPosX - static_cast<float>(entry->hotspotX);
        const float y = menuCursor->cursorPosY - static_cast<float>(entry->hotspotY);

        a_renderData.spriteBatch->Draw(entry->srv.Get(),
                                       ::DirectX::SimpleMath::Vector2(x, y),
                                       ::DirectX::Colors::White);
    }
}
