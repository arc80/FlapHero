#include <flapGame/Core.h>
#include <flapGame/Puffs.h>
#include <flapGame/GameState.h>

namespace flap {

bool Puffs::update(float dt) {
    this->time += dt * Rate;
    return this->time < 1.f;
};

void Puffs::addInstances(Array<PuffShader::InstanceData>& instances) const {
    Random r{this->seed};
    constexpr u32 numPuffs = 8;
    float offset = 1.f - powf(1.f - this->time, 1.5f);
    Float3 basePos = this->pos + Float3{0.3f, 0, 1.f} * (offset * 0.9f);
    float distance = 1.f - powf(1.f - this->time, 3.f);
    float alpha = 1.f - powf(this->time, 1.2f);
    alpha *= min(1.f, this->time * 4.f);
    Float3 axis = Float3{0.2f, -0.4f, 1.f}.normalized();
    float scale = mix(0.3f, 1.f, 1.f - powf(1.f - this->time, 3.f));
    for (u32 i = 0; i < numPuffs; i++) {
        float rotFactor = (s32(r.next32() & 2) - 1) * (r.nextFloat() + 1.f) * 0.5f;
        PuffShader::InstanceData& inst = instances.append();
        Float3 pos = basePos + Quaternion::fromAxisAngle(axis, i * 2.f * Pi / numPuffs) *
                                     Float3{mix(0.5f, 1.3f, distance), 0, 0};
        inst.modelToWorld = Float4x4::makeTranslation(pos) *
                            Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) *
                            Float4x4::makeRotation({0, 0, 1}, (this->time + 5.f) * rotFactor) *
                            Float4x4::makeScale(0.8f * scale);
        inst.colorAlpha = {this->time, alpha * 0.9f};
    }
}

} // namespace flap
