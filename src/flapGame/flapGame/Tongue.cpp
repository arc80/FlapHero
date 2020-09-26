#include <flapGame/Core.h>
#include <flapGame/Tongue.h>

namespace flap {

Tongue::Tongue() {
    for (u32 i = 0; i < 4; i++) {
        this->particles[0].append(Float3{0.4f * i, 0, 0});
    }
    this->particles[1] = this->particles[0];
}

void Tongue::update(float dt) {
    s32 iters = 1;
    float gravity = 0.01f;
    for (; iters > 0; iters--) {
        this->curIndex = 1 - this->curIndex;
        auto& curParts = this->particles[this->curIndex];
        auto& prevParts = this->particles[1 - this->curIndex];
        PLY_ASSERT(curParts.numItems() == prevParts.numItems());

        // Set first particle
        curParts[0] = {0, 0, 0};

        for (u32 i = 1; i < curParts.numItems(); i++) {
            Float3 step = prevParts[i] - curParts[i];
            step *= 0.98f;
            curParts[i] = prevParts[i] + step;
            curParts[i].z -= gravity;
        }

        // Constraints
        float segLen = 0.4f;
        for (u32 i = 1; i < curParts.numItems(); i++) {
            Float3* part = &curParts[i];

            // segment length
            float sl = segLen;

            // FIXME: div by zero
            Float3 dir = (part[0] - part[-1]).normalized();
            part[0] = part[-1] + dir * sl;
        }
    }
}

} // namespace flap
