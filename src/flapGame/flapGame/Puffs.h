#pragma once
#include <flapGame/Core.h>
#include <flapGame/Shaders.h>

namespace flap {

struct Puffs {
    Float3 pos = {0, 0, 0};
    float time = 0.f;
    u32 seed = 0;

    PLY_INLINE Puffs(const Float3& pos, u32 seed) : pos{pos}, seed{seed} {
    }
    bool update(float dt);
    void addInstances(Array<PuffShader::InstanceData>& instances) const;
};

} // namespace flap
