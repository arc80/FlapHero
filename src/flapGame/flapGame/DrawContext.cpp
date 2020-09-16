#include <flapGame/Core.h>
#include <flapGame/DrawContext.h>

namespace flap {

ViewportFrustum fitFrustumInViewport(const Rect& viewport, const Rect& frustum,
                                     const Rect& bounds2D) {
    PLY_ASSERT(!frustum.isEmpty());
    PLY_ASSERT(!viewport.isEmpty());

    if (frustum.width() * viewport.height() >= frustum.height() * viewport.width()) {
        // Frustum aspect is wider than (or equal to) viewport aspct
        float fitViewportHeight = viewport.width() * frustum.height() / frustum.width();
        float halfExcess = (viewport.height() - fitViewportHeight) * 0.5f;
        return {expand(viewport, {0, -halfExcess}), frustum, bounds2D};
    } else {
        // Frustum aspect is taller than viewport aspect
        float fitViewportWidth = viewport.height() * frustum.width() / frustum.height();
        float halfExcess = (viewport.width() - fitViewportWidth) * 0.5f;
        return {expand(viewport, {-halfExcess, 0}), frustum, bounds2D};
    }
}

DrawContext* DrawContext::instance_ = nullptr;

} // namespace flap
