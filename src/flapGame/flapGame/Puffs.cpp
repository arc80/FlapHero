#include <flapGame/Core.h>
#include <flapGame/Puffs.h>
#include <flapGame/GameState.h>
#include <flapGame/DrawContext.h>

namespace flap {

bool Puffs::update(float dt) {
    this->time += dt * Rate;
    return this->time < 1.f;
};

void Puffs::addInstances(Array<PuffShader::InstanceData>& instances) const {
    Random r{this->seed};
    constexpr u32 numPuffs = 10;
    float t = min(1.f, this->time + DrawContext::instance()->fracTime * Rate);
    float offset = 1.f - powf(1.f - t, 1.5f);
    Float3 basePos = this->pos + Float3{0.3f, 0, 1.f} * (offset * 0.7f + 0.5f);
    float distance = 1.f - powf(1.f - t, 2.f);
    float alpha = 1.f - powf(t, 1.2f);
    alpha *= min(1.f, t * 2.5f);
    Float3 axis = Float3{0.2f, -0.4f, 1.f}.normalized();
    float scale = mix(0.3f, 1.f, 1.f - powf(1.f - t, 3.f));
    float side = 1.f;
    for (u32 i = 0; i < numPuffs; i++) {
        float rotFactor = (s32(r.next32() & 2) - 1) * (r.nextFloat() + 1.f) * 0.6f;
        PuffShader::InstanceData& inst = instances.append();
        Float3 pos =
            basePos +
            Float3x3::makeScale({1.f, 2.f, 1.f}) *
                (Quaternion::fromAxisAngle(axis, Pi * ((i + 0.5f) * side / numPuffs + 0.5f)) *
                 Float3{mix(0.4f, 1.2f, distance), 0, 0});
        inst.modelToWorld = Float4x4::makeTranslation(pos) *
                            Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) *
                            Float4x4::makeRotation({0, 0, 1}, (t + 15.f) * rotFactor) *
                            Float4x4::makeScale(0.6f * scale);
        inst.colorAlpha = {t, alpha * 0.7f};
        side *= -1.f;
    }
}

} // namespace flap
