#pragma once
#include <flapGame/Core.h>
#include <flapGame/Shaders.h>

namespace flap {

struct Sweat {
    static constexpr float Rate = 1.3f;
    float time = 0.f;
    u32 seed = 0;

    PLY_INLINE Sweat(u32 seed = 1) : seed{seed} {
    }
    bool update(float dt);
    void addInstances(const Float4x4& birdToViewport, Array<StarShader::InstanceData>& instances) const;
};

} // namespace flap
