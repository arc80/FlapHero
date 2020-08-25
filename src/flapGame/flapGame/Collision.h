#pragma once
#include <flapGame/Core.h>

namespace flap {

struct SphCylCollResult {
    enum Type {
        None,
        Inside,
        Side,
        CapPlane,
        CapEdge,
    };

    Float3 pos = {0, 0, 0};
    Float3 norm = {0, 0, 0};
    float penetrationDepth = 0.f;
};

SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   float cylRadius, SphCylCollResult* result);
SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   const Float3x4& cylToWorld, float cylRadius,
                                                   SphCylCollResult* result);

} // namespace flap
