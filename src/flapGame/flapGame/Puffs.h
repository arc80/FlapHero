#pragma once
#include <flapGame/Core.h>
#include <flapGame/Shaders.h>

namespace flap {

struct Puffs {
    Float3 pos = {0, 0, 0};
    Float3 dir = {0, 0, 1};
    float time = 0.f;
    u32 seed = 0;
    bool big = false;

    PLY_INLINE Puffs(const Float3& pos, u32 seed,
                     const Float3& dir = Float3{0.3f, 0, 1.f}.normalized(), bool big = false)
        : pos{pos}, dir{dir}, seed{seed}, big{big} {
    }
    bool update(float dt);
    void addInstances(Array<PuffShader::InstanceData>& instances) const;

    PLY_INLINE float getRate() const {
        return this->big ? 0.7f : 1.3f;
    }
};

} // namespace flap
