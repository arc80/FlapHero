#include <flapGame/Core.h>
#include <flapGame/Sweat.h>
#include <flapGame/DrawContext.h>

namespace flap {

bool Sweat::update(float dt) {
    this->time = min(2.f, this->time + dt * 2.f);
    return this->time < 1.4f;
};

void Sweat::addInstances(const Float4x4& birdToViewport,
                         Array<StarShader::InstanceData>& instances) const {
    Random r{this->seed};
    auto addAtAngle = [&](float a, float d, float lag) {
        float t = min(1.f, this->time + DrawContext::instance()->fracTime * 2.f) + lag;
        if (t < 0 || t > 1.f)
            return;
        float distance = interpolateCubic(1.3f, 1.8f, 2.3f, 2.3f, t);
        float alpha = clamp(mix(1.25f, 0.f, t), 0.f, 1.f);
        float size = clamp(mix(0.f, 2.f, t), 0.f, 1.f);
        float c = 4.f * t - 6.f * t * t - 0.8f;
        StarShader::InstanceData& inst = instances.append();
        inst.color = {1, 1, 1, alpha};
        Float2 ofs = Complex::fromAngle(a) * distance * d;
        inst.modelToViewport = birdToViewport * Float4x4::makeTranslation({ofs.x + 0.02f, 0, ofs.y + c}) *
                               Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) *
                               Float4x4::makeScale(0.2f * size);
    };
    for (u32 j = 0; j < 2; j++) {
        float sideLag = r.nextFloat() * 0.2f;
        for (u32 i = 0; i < 3; i++) {
            float a = i * 0.2f + 0.4f + mix(-0.04f, 0.04f, r.nextFloat());
            float d = (i == 1 ? 1.15f : 1.f) + mix(-0.05f, 0.05f, r.nextFloat());;
            float lag = sideLag + r.nextFloat() * 0.1f;
            addAtAngle(j ? Pi - a : a, d, lag);
        }
    }
}

} // namespace flap
