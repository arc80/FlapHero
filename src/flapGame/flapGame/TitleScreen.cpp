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
                   mix(0.4f, 1.f, powf(star.z, 0.5f));
        star.angle[0] = mix(0.f, 2.f * Pi, random.nextFloat());
        star.angle[1] = star.angle[0];
        star.avel = mix(2.f, 3.5f, random.nextFloat()) * (s32(random.next32() & 2) - 1);
        star.color = Float3{mix(0.8f, 1.f, random.nextFloat()), mix(0.8f, 1.f, random.nextFloat()),
                            mix(0.3f, 0.8f, random.nextFloat())};
        starSys->countdown = 0.02f;
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

float wrapInterval(ArrayView<float> arr, float delta, float positiveRange) {
    arr[0] = arr[1];
    arr[1] += delta;
    float w = wrap(arr[1], positiveRange);
    float dw = w - arr[1];
    arr[0] += dw;
    arr[1] = w;
    return dw;
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
    float dw = wrapInterval({titleScreen->hypnoZoom, 2}, dt * 1.2f, 12.f);
    float adjustAngle = dw * (2.f * Pi / 48.f);
    titleScreen->hypnoAngle[0] -= adjustAngle;
    titleScreen->hypnoAngle[1] -= adjustAngle;
    wrapInterval({titleScreen->hypnoAngle, 2}, dt * 1.2f, Pi * 2.f);
}

} // namespace flap
