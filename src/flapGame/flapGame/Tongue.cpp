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

Float3 constrainToCone(const Float3& ray, const Float3& fwd, const Float2& coneCS) {
    float d = dot(ray, fwd);
    Float3 perp = ray - fwd * d;
    float pL = perp.length();
    if (d < 0.1f) {
        if (pL < 1e-4f) {
            perp = anyPerp(fwd);
            pL = 1.f;
        }
        return fwd * coneCS.x + perp * (coneCS.y / pL);
    } else {
        if (pL > 1e-4f) {
            Float3 norm = -fwd * coneCS.y + perp * (coneCS.x / pL);
            PLY_ASSERT(norm.isUnit());
            float nd = dot(norm, ray);
            if (nd > 0) {
                return ray - norm * nd;
            }
        }
    }
    return ray;
}

void Tongue::update(const Float3& correction, const Quaternion& birdToWorldRot, float dt,
                    bool applySidewaysForce, float limitZ) {
    const Assets* a = Assets::instance;
    s32 iters = 1;
    for (; iters > 0; iters--) {
        this->curIndex = 1 - this->curIndex;
        auto& curState = this->states[this->curIndex];
        auto& prevState = this->states[1 - this->curIndex];
        PLY_ASSERT(curState.pts.numItems() == prevState.pts.numItems());

        // Set first particles
        curState.rootRot = birdToWorldRot * a->bad.tongueRootRot;
        curState.pts[0] = birdToWorldRot * a->bad.tongueBones[0].midPoint;
        Float3 fwd = curState.rootRot.rotateUnitY();

        for (u32 i = 1; i < curState.pts.numItems(); i++) {
            Float3 step = prevState.pts[i] - curState.pts[i] + correction * 0.5f;
            // Wind resistance
            step *= 0.98f;
            float L = step.length();
            constexpr float R = 0.0015f;
            if (L > R) {
                step -= step * (R / L);
            } else {
                step = {0, 0, 0};
            }
            step.z -= 0.006f;
            if (applySidewaysForce) {
                step += Float3{-1, -1, 0} * 0.002f;
            }
            curState.pts[i] = prevState.pts[i] + step;
        }

        // Constrain second point to cone around the mouth
        {
            float angle = 50.f * Pi / 180.f;
            Float2 coneCS = {cosf(angle), sinf(angle)};
            Float3 ray = curState.pts[1] - curState.pts[0];
            Float3 wide = curState.rootRot.rotateUnitX();
            constexpr float wideScale = 3.f;
            ray += wide * (dot(wide, ray) * (1.f / wideScale - 1.f));
            ray = constrainToCone(ray, fwd, coneCS);
            ray += wide * (dot(wide, ray) * (wideScale - 1.f));
            curState.pts[1] = curState.pts[0] + ray;
        }

        // Constraints
        Float3 prevSeg = curState.pts[1] - curState.pts[0];
        for (u32 i = 1; i < curState.pts.numItems(); i++) {
            Float3* part = &curState.pts[i];

            // segment length
            float segLen = (a->bad.tongueBones[i - 1].length + a->bad.tongueBones[i].length) * 0.5f;
            Float3 dir = (part[0] - part[-1]).safeNormalized(prevSeg.normalized());
            prevSeg = dir * segLen;
            part[0] = part[-1] + prevSeg;

            // limit Z
            if (part[0].z < limitZ) {
                part[0].z = limitZ;
            }
        }
    }
}

} // namespace flap
