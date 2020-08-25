#include <flapGame/Core.h>
#include <flapGame/TitleScreen.h>
#include <flapGame/GameState.h>
#include <soloud.h>

namespace flap {

void timeStep(TitleRotator* rot) {
    UpdateContext* uc = UpdateContext::instance();
    Random& random = uc->gs->random;
    float dt = uc->gs->outerCtx->simulationTimeStep;

    rot->time += dt;
    if (rot->state == TitleRotator::Waiting) {
        if (rot->time >= TitleRotator::WaitTime) {
            rot->state = TitleRotator::Tilting;
            rot->startNorm = rot->endNorm;
            const float minDeltaAngle = Pi / 3.f;
            rot->endAngle = wrap(
                rot->endAngle + mix(minDeltaAngle, 2.f * Pi - minDeltaAngle, random.nextFloat()),
                2.f * Pi);
            rot->endNorm = Float3{Complex::fromAngle(rot->endAngle), 1.2f}.normalized();
            rot->time = 0;
        }
    } else {
        if (rot->time >= TitleRotator::TiltTime) {
            rot->state = TitleRotator::Waiting;
            rot->time = 0;
        }
    }
}

void timeStep(StarSystem* starSys) {
    UpdateContext* uc = UpdateContext::instance();
    Random& random = uc->gs->random;
    float dt = uc->gs->outerCtx->simulationTimeStep;

    starSys->countdown -= dt;
    if (starSys->countdown <= 0) {
        StarSystem::Star& star = starSys->stars.append();
        star.pos[0] = {mix(-0.5f, 0.5f, random.nextFloat()), -1.6f};
        star.pos[1] = star.pos[0];
        star.z = random.nextFloat();
        star.vel = Float2{mix(-1.f, 1.f, random.nextFloat()), mix(3.0f, 3.0f, random.nextFloat())} *
                   mix(0.4f, 1.f, star.z);
        star.angle[0] = mix(0.f, 2.f * Pi, random.nextFloat());
        star.angle[1] = star.angle[0];
        star.avel = mix(2.f, 3.5f, random.nextFloat()) * (s32(random.next32() & 2) - 1);
        star.color = Float3{mix(0.6f, 1.f, random.nextFloat()), mix(0.6f, 1.f, random.nextFloat()),
                            mix(0.3f, 0.7f, random.nextFloat())};
        starSys->countdown = 0.015f;
    }
    for (u32 i = 0; i < starSys->stars.numItems();) {
        StarSystem::Star& star = starSys->stars[i];
        star.vel.y -= dt * 1.5f;
        star.pos[0] = star.pos[1];
        star.pos[1] = star.pos[0] + star.vel * dt;
        star.angle[0] = star.angle[1];
        float a1 = star.angle[0] + star.avel * dt;
        star.angle[1] = wrap(a1, 2 * Pi);
        star.angle[0] += (star.angle[1] - a1);
        if (star.pos[1].y < -2.f) {
            starSys->stars.eraseQuick(i);
        } else {
            i++;
        }
    }
}

void updateTitleScreen(TitleScreen* titleScreen) {
    UpdateContext* uc = UpdateContext::instance();
    float dt = uc->gs->outerCtx->simulationTimeStep;

    titleScreen->promptTime += dt;
    if (titleScreen->promptTime >= (titleScreen->showPrompt ? 0.4f : 0.16f)) {
        titleScreen->showPrompt = !titleScreen->showPrompt;
        titleScreen->promptTime = 0.f;
    }
    timeStep(&titleScreen->titleRot);
    timeStep(&titleScreen->starSys);
}

} // namespace flap
