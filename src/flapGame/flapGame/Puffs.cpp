#include <flapGame/Core.h>
#include <flapGame/Puffs.h>

namespace flap {

bool Puffs::update(float dt) {
    this->time += dt * 1.f;
    return this->time < 1.f;
};

void Puffs::addInstances(Array<PuffShader::InstanceData>& instances) const {
    Random r{this->seed};
    constexpr u32 numPuffs = 7;
    float distance = 1.f - powf(1.f - this->time, 5.f);
    Float3 axis = Float3{0, -0.5f, 1.f}.normalized();
    float scale = distance;
    for (u32 i = 0; i < numPuffs; i++) {
        float rotFactor = (s32(r.next32() & 2) - 1) * mix(0.3f, 0.6f, r.nextFloat());
        float angleDev = r.nextFloat() * 0.4f;
        PuffShader::InstanceData& inst = instances.append();
        Float3 pos =
            this->pos + Quaternion::fromAxisAngle(axis, i * 2.f * Pi / numPuffs + angleDev + 0.1f) *
                            Float3{distance * 1.1f, 0, 0};
        inst.modelToWorld = Float4x4::makeTranslation(pos) *
                            Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) *
                            Float4x4::makeRotation({0, 0, 1}, (this->time + 5.f) * rotFactor) *
                            Float4x4::makeScale(0.9f * scale);
        inst.colorAlpha = {this->time, mix(0.8f, 0.2f, this->time)};
    }
}

} // namespace flap
