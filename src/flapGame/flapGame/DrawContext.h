#pragma once
#include <flapGame/Core.h>

namespace flap {

struct GameState;

struct ViewportFrustum {
    Rect viewport; // Viewport extents relative to render target
    Rect frustum;  // Frustum rect on z = -1 plane
    Rect bounds2D; // Logical 2D rect; used to determine text height

    // If rendering to a non-retina display, bounds2D == viewport

    PLY_NO_DISCARD ViewportFrustum snappedToPixels() const {
        Rect qvp = {roundNearest(this->viewport.mins), roundNearest(this->viewport.maxs)};
        Rect fracs = this->viewport.unmix(qvp);
        return {qvp, this->frustum.mix(fracs), this->bounds2D.mix(fracs)};
    }

    PLY_NO_DISCARD ViewportFrustum clip(const Rect& clipViewport) const {
        Rect clipped = intersect(this->viewport, clipViewport);
        Rect fracs = this->viewport.unmix(clipped);
        return {clipped, this->frustum.mix(fracs), this->bounds2D.mix(fracs)};
    }
};

ViewportFrustum fitFrustumInViewport(const Rect& viewport, const Rect& frustum,
                                     const Rect& bounds2D);
ViewportFrustum getViewportFrustum(const Float2& fbSize);

struct DrawContext {
    const GameState* gs = nullptr;
    ViewportFrustum vf;
    ViewportFrustum fullVF;
    float renderDT = 0;
    float fracTime = 0;
    float intervalFrac = 0;
    Rect visibleExtents = {{0, 0}, {0, 0}};

    static DrawContext* instance_;
    static PLY_INLINE DrawContext* instance() {
        PLY_ASSERT(instance_);
        return instance_;
    };
};

} // namespace flap
