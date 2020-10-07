#include <flapGame/Core.h>
#include <flapGame/Puffs.h>
#include <flapGame/GameState.h>
#include <flapGame/DrawContext.h>

namespace flap {

bool Puffs::update(float dt) {
    this->time += dt * this->getRate();
    return this->time < 1.f;
};

void Puffs::addInstances(Array<PuffShader::InstanceData>& instances) const {
    Random r{this->seed};
    Float3 axis = [&] {
        Float3 toward = this->dir.y < 0.1f ? Float3{0, -1, 0} : Float3{0, 1, 0};
        float skew = 0.3f;
        return (this->dir + toward * skew).normalized();
    }();
    u32 numPuffs = 10;
    float t = min(1.f, this->time + DrawContext::instance()->fracTime * this->getRate());
    float offset = 1.f - powf(1.f - t, 1.5f);
    if (big) {
        offset = (1.f - powf(1.f - t, 4.f)) * 2.f + t;
        offset *= 0.5f;
        numPuffs = 12;
    } else {
        offset = 1.044f * (offset * 0.7f + 0.5f);
    }
    Float3 basePos = this->pos + this->dir * offset;
    float distance = 1.f - powf(1.f - t, 2.f);
    float alpha = 1.f - powf(t, 1.2f);
    alpha *= min(1.f, t * 2.5f);
    float scale = mix(0.3f, 1.f, 1.f - powf(1.f - t, 3.f));
    if (big) {
        scale *= 1.7f;
        distance *= 1.7f;
        alpha = clamp(min(t * 4.f, powf((1.f - t) * 1.4f, 1.5f)), 0.f, 0.8f);
    }
    float side = 1.f;
    Float3x3 ringToWorld = makeBasis(axis, {0, 1, 0}, Axis3::ZPos, Axis3::YPos);
    for (u32 i = 0; i < numPuffs; i++) {
        float rotFactor = (s32(r.next32() & 2) - 1) * (r.nextFloat() + 1.f) * 0.6f;
        if (big) {
            rotFactor *= 1.3f;
        }
        PuffShader::InstanceData& inst = instances.append();
        Float3 pos =
            basePos +
            Float3x3::makeScale({1.f, 2.f, 1.f}) *
                (Quaternion::fromAxisAngle(axis, Pi * ((i + 0.5f) * side / numPuffs + 0.5f)) *
                 (ringToWorld * Float3{mix(0.4f, 1.2f, distance), 0, 0}));
        inst.modelToWorld = Float4x4::makeTranslation(pos) *
                            Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) *
                            Float4x4::makeRotation({0, 0, 1}, (t + 15.f) * rotFactor) *
                            Float4x4::makeScale(0.6f * scale);
        inst.colorAlpha = {t, alpha * 0.7f};
        side *= -1.f;
    }
}

} // namespace flap
