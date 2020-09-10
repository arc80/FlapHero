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

Float2 circularRand(Random& random) {
    float r = random.nextFloat();
    float a = random.nextFloat() * 2.f * Pi;
    return (Complex::fromAngle(a) * mix(0.8f, 0.f, r * r)) * 0.5f + 0.5f;
};

void timeStep(StarSystem* starSys) {
    UpdateContext* uc = UpdateContext::instance();
    Random& random = uc->gs->random;
    float dt = uc->gs->outerCtx->simulationTimeStep * 1.1f;

    starSys->countdown -= dt;
    if (starSys->countdown <= 0) {
        if (starSys->burstNumber > 0) {
            auto randRange = [&](float v, float bias, float spread) {
                float lo = mix(0.f, 1.f - spread, bias);
                return lo + v * spread;
            };
            Float2 cr = circularRand(random);
            StarSystem::Star& star = starSys->stars.append();
            star.pos[0] = {0.f, -1.6f};
            star.pos[1] = star.pos[0];
            star.z = randRange(cr.y, starSys->burstPos.y, 0.3f);
            star.vel = Float2{mix(-1.1f, 1.1f, randRange(cr.x, starSys->burstPos.x, 0.2f)),
                              3.1f * mix(0.7f, 1.f, star.z)};
            star.angle[0] = mix(0.f, 2.f * Pi, random.nextFloat());
            star.angle[1] = star.angle[0];
            star.avel = mix(1.5f, 2.5f, random.nextFloat()) * (s32(random.next32() & 2) - 1);
            star.brightness = random.nextFloat();
            starSys->countdown = 0.016f;
            starSys->burstNumber--;
        } else {
            starSys->countdown = 0.35f;
            starSys->burstNumber = StarSystem::StarsPerBurst;
            starSys->side *= -1.f;
            starSys->burstPos = {0.5f + 0.5f * starSys->side * random.nextFloat(),
                                 1.f - powf(random.nextFloat(), 5.f)};
        }
    }
    for (u32 i = 0; i < starSys->stars.numItems();) {
        StarSystem::Star& star = starSys->stars[i];
        star.life[0] = star.life[1];
        star.life[1] += dt;
        star.vel.y = approach(star.vel.y, -0.3f, dt * 1.5f);
        star.vel.x *= 0.998f;
        star.pos[0] = star.pos[1];
        star.pos[1] = star.pos[0] + star.vel * dt;
        star.angle[0] = star.angle[1];
        float a1 = star.angle[0] + star.avel * dt;
        star.angle[1] = wrap(a1, 2 * Pi);
        star.angle[0] += (star.angle[1] - a1);
        if (star.pos[1].y < -2.f || star.life[1] >= 5.f) {
            starSys->stars.eraseQuick(i);
        } else {
            i++;
        }
    }
}

StarSystem::StarSystem() {
    for (u32 i = 0; i < 500; i++) {
        timeStep(this);
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
    float dw = wrapInterval({titleScreen->hypnoZoom, 2}, dt * 1.8f, 12.f);
    float adjustAngle = dw * (2.f * Pi / 48.f);
    titleScreen->hypnoAngle[0] -= adjustAngle;
    titleScreen->hypnoAngle[1] -= adjustAngle;
    wrapInterval({titleScreen->hypnoAngle, 2}, dt * 1.3f, Pi * 2.f);
    wrapInterval({titleScreen->raysAngle, 2}, dt * 0.5f, Pi * 2.f);
}

} // namespace flap
