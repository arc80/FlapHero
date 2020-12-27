#include <flapGame/Core.h>
#include <flapGame/TitleScreen.h>
#include <flapGame/GameState.h>

#if PLY_TARGET_WIN32
#include <shellapi.h>
#elif PLY_TARGET_IOS || PLY_TARGET_ANDROID
extern "C"
void openOpenSourcePage();
#endif

namespace flap {

TitleRotator::TitleRotator() {
    this->endAngle = 0.9f;
    this->startNorm = Float3{Complex::fromAngle(this->endAngle), 1.65f}.normalized();
    this->endNorm = this->startNorm;
}

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
            rot->endNorm = Float3{Complex::fromAngle(rot->endAngle), 1.65f}.normalized();
            rot->time = 0;
        }
    } else {
        if (rot->time >= TitleRotator::TiltTime) {
            rot->state = TitleRotator::Waiting;
            rot->time = 0;
        }
    }
}

Float3 sphericalRand(Random& random) {
    float z = mix(-1.f, 1.f, random.nextFloat());
    float r = sqrtf(max(0.f, 1.f - z * z));
    float a = mix(0.f, 2.f * Pi, random.nextFloat());
    return Float3{Complex::fromAngle(a) * r, z};
};

Float3 circularRand(Random& random) {
    float r = sqrtf(random.nextFloat());
    float a = mix(0.f, 2.f * Pi, random.nextFloat());
    return Float3{Complex::fromAngle(a) * r, 0.f};
}

Array<Float3> getMitchellSpherePoints(Random& random, u32 numPts) {
    Array<Float3> result;
    result.reserve(numPts);
    result.append(circularRand(random));
    for (u32 i = 1; i < numPts; i++) {
        Float3 best = {0, 0, 0};
        float bestDist = -1.f;
        for (u32 j = 1; j < 4; j++) {
            Float3 cand = circularRand(random);
            float minDist = INFINITY;
            for (u32 k = 0; k < i; k++) {
                minDist = min(minDist, (result[k] - cand).length2());
            }
            if (minDist > bestDist) {
                best = cand;
                bestDist = minDist;
            }
        }
        result.append(best);
    }
    return result;
}

void timeStep(StarSystem* starSys) {
    UpdateContext* uc = UpdateContext::instance();
    Random& random = uc->gs->random;
    float dt = uc->gs->outerCtx->simulationTimeStep;

    starSys->countdown -= dt;
    if (starSys->countdown <= 0) {
        Float2 burstPos = {0.5f + 0.5f * starSys->side * random.nextFloat(),
                           1.1f - 1.1f * powf(random.nextFloat(), 1.5f) - 0.25f * random.nextFloat()};
        Array<Float3> sphPoints = getMitchellSpherePoints(random, 15);
        for (const Float3& sr : sphPoints) {
            StarSystem::Star& star = starSys->stars.append();
            star.pos[0] = {Rect{{-0.8f, -0.5f}, {0.8f, 1.1f}}.mix(burstPos), 0.f};
            star.pos[1] = star.pos[0];
            star.vel = sr * 1.1f;
            star.vel = {star.vel.asFloat2() + Rect{{-1.0f, 0.8f}, {1.0f, 1.3f}}.mix(burstPos),
                        star.vel.z};
            star.angle[0] = mix(0.f, 2.f * Pi, random.nextFloat());
            star.angle[1] = star.angle[0];
            star.avel = mix(1.f, 2.4f, random.nextFloat()) * (s32(random.next32() & 2) - 1);
            float c = random.nextFloat();
            star.color =
                mix(Float4{1.0f, 1.0f, 0.2f, 0.87f}, Float4{1.0f, 1.0f, 1.0f, 0.97f}, c * c * 0.8f);
        }
        starSys->countdown = 0.25f;
        starSys->side *= -1.f;
    }
    for (u32 i = 0; i < starSys->stars.numItems();) {
        StarSystem::Star& star = starSys->stars[i];
        star.life[0] = star.life[1];
        star.life[1] += dt;
        star.vel *= powf(0.985f, star.vel.length());
        star.vel.y = approach(star.vel.y, -0.45f, dt * 0.35f);
        star.pos[0] = star.pos[1];
        star.pos[1] = star.pos[0] + star.vel * dt;
        star.angle[0] = star.angle[1];
        float a1 = star.angle[0] + star.avel * dt;
        star.angle[1] = wrap(a1, 2 * Pi);
        star.angle[0] += (star.angle[1] - a1);
        if (star.pos[1].y < -2.f || star.life[1] >= 3.3f) {
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
    titleScreen->osb.angle = wrap(titleScreen->osb.angle + dt * 0.3f, Pi * 2.f);
    if (titleScreen->osb.button.update(dt)) {
        titleScreen->osb.pulsateTime = wrap(titleScreen->osb.pulsateTime + dt, 2.f);
    } else {
        titleScreen->osb.pulsateTime = 0;
    }
    if (titleScreen->osb.button.wasClicked && titleScreen->osb.button.timeSinceClicked >= 0.2f) {
        titleScreen->osb.button.wasClicked = false;
        titleScreen->osb.button.timeSinceClicked = 0.f;
#if PLY_TARGET_WIN32
        ShellExecute(NULL, "open", "https://arc80.com/flaphero/opensource", NULL, NULL, SW_SHOWNORMAL);
#elif PLY_TARGET_IOS || PLY_TARGET_ANDROID
        openOpenSourcePage();
#endif
    }
}

} // namespace flap
