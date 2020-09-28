#include <flapGame/Core.h>
#include <flapGame/Tongue.h>
#include <flapGame/Assets.h>

namespace flap {

Tongue::Tongue() {
    const Assets* a = Assets::instance;
    for (u32 i = 0; i < a->bad.tongueBones.numItems(); i++) {
        this->states[0].pts.append(Float3{0.4f * i, 0, 0});
    }
    this->states[1] = this->states[0];
}

void Tongue::update(const QuatPos& root, float dt) {
    s32 iters = 1;
    float gravity = 0.0002f;
    for (; iters > 0; iters--) {
        this->curIndex = 1 - this->curIndex;
        auto& curState = this->states[this->curIndex];
        auto& prevState = this->states[1 - this->curIndex];
        PLY_ASSERT(curState.pts.numItems() == prevState.pts.numItems());

        // Set first particles
        curState.rootRot = root.quat;
        curState.pts[0] = root.pos;

        for (u32 i = 1; i < curState.pts.numItems(); i++) {
            Float3 step = prevState.pts[i] - curState.pts[i];
            step *= 0.998f;
            curState.pts[i] = prevState.pts[i] + step;
            curState.pts[i].z -= gravity;
        }

        // Constraints
        float segLen = 0.4f;
        for (u32 i = 1; i < curState.pts.numItems(); i++) {
            Float3* part = &curState.pts[i];

            // segment length
            float sl = segLen;

            // FIXME: div by zero
            Float3 dir = (part[0] - part[-1]).normalized();
            part[0] = part[-1] + dir * sl;
        }
    }
}

} // namespace flap
