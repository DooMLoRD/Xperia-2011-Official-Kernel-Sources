/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TiledTexture_h
#define TiledTexture_h

#include "BaseTile.h"
#include "BaseTileTexture.h"
#include "ClassTracker.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "SkRegion.h"
#include "TextureOwner.h"
#include "TilePainter.h"
#include "UpdateManager.h"

class SkCanvas;

namespace WebCore {

class UpdateManager;
class PaintedSurface;

class TiledTexture : public TilePainter {
public:
    TiledTexture(PaintedSurface* surface)
        : m_surface(surface)
        , m_prevTileX(0)
        , m_prevTileY(0)
        , m_scale(1)
        , m_swapWhateverIsReady(false)
    {
        m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
        ClassTracker::instance()->increment("TiledTexture");
#endif
    }
    virtual ~TiledTexture()
    {
#ifdef DEBUG_COUNT
        ClassTracker::instance()->decrement("TiledTexture");
#endif
        removeTiles();
    };

    void prepare(GLWebViewState* state, float scale, bool repaint,
                 bool startFastSwap, IntRect& visibleArea);
    bool draw();

    void prepareTile(bool repaint, int x, int y);
    void update(const SkRegion& dirtyArea, SkPicture* picture);

    BaseTile* getTile(int x, int y);

    void removeTiles();
    void discardTextures();
    bool owns(BaseTileTexture* texture);

    // TilePainter methods
    bool paint(BaseTile* tile, SkCanvas*, unsigned int*);
    virtual void paintExtra(SkCanvas*);
    virtual const TransformationMatrix* transform();

    float scale() { return m_scale; }
    bool ready();

    PaintedSurface* surface() { return m_surface; }

private:
    bool tileIsVisible(BaseTile* tile);

    UpdateManager m_updateManager;

    PaintedSurface* m_surface;
    Vector<BaseTile*> m_tiles;

    // tile coordinates in viewport, set in prepare()
    IntRect m_area;

    SkRegion m_dirtyRegion;

    int m_prevTileX;
    int m_prevTileY;
    float m_scale;

    bool m_swapWhateverIsReady;
};

class DualTiledTexture {
public:
    DualTiledTexture(PaintedSurface* surface);
    ~DualTiledTexture();
    void prepare(GLWebViewState* state, float scale, bool repaint,
                 bool startFastSwap, IntRect& area);
    void swap();
    bool draw();
    void update(const SkRegion& dirtyArea, SkPicture* picture);
    bool owns(BaseTileTexture* texture);
private:
    // Delay before we schedule a new tile at the new scale factor
    static const double s_zoomUpdateDelay = 0.2; // 200 ms

    TiledTexture* m_frontTexture;
    TiledTexture* m_backTexture;
    TiledTexture* m_textureA;
    TiledTexture* m_textureB;
    float m_scale;
    float m_futureScale;
    double m_zoomUpdateTime;
    bool m_zooming;
    IntRect m_preZoomVisibleArea;
};

} // namespace WebCore

#endif // TiledTexture_h
