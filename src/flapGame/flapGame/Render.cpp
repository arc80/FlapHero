#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>

namespace flap {

void drawRoundedRect(const TexturedShader* shader, const Float4x4& modelToViewport,
                     GLuint textureID, const Float4 color, const Rect& bounds, float r) {
    Array<VertexPT> verts;
    Array<u16> indices;

    auto addQuad = [&](const Rect& r, const Rect& tc) {
        u32 b = verts.numItems();
        verts.append({{r.mins, 0}, tc.mins});
        verts.append({{r.bottomRight(), 0}, tc.bottomRight()});
        verts.append({{r.maxs, 0}, tc.maxs});
        verts.append({{r.topLeft(), 0}, tc.topLeft()});
        indices.extend({u16(b), u16(b + 1), u16(b + 2), u16(b + 2), u16(b + 3), u16(b)});
    };

    // Bottom row
    addQuad({bounds.mins, bounds.mins + Float2{r, r}}, {{0, 1}, {1, 0}});
    addQuad({bounds.mins + Float2{r, 0}, bounds.bottomRight() + Float2{-r, r}}, {{1, 1}, {1, 0}});
    addQuad({bounds.bottomRight() + Float2{-r, 0}, bounds.bottomRight() + Float2{0, r}},
            {{1, 1}, {0, 0}});

    // Middle row
    addQuad({bounds.mins + Float2{0, r}, bounds.topLeft() + Float2{r, -r}}, {{0, 0}, {1, 0}});
    addQuad({bounds.mins + Float2{r, r}, bounds.maxs + Float2{-r, -r}}, {{1, 0}, {1, 0}});
    addQuad({bounds.bottomRight() + Float2{-r, r}, bounds.maxs + Float2{0, -r}}, {{1, 0}, {0, 0}});

    // Top row
    addQuad({bounds.topLeft() + Float2{0, -r}, bounds.topLeft() + Float2{r, 0}}, {{0, 0}, {1, 1}});
    addQuad({bounds.topLeft() + Float2{r, -r}, bounds.maxs + Float2{-r, 0}}, {{1, 0}, {1, 1}});
    addQuad({bounds.maxs + Float2{-r, -r}, bounds.maxs}, {{1, 0}, {0, 1}});

    shader->draw(modelToViewport, textureID, color, verts.view(), indices.view());
}

void drawScoreSign(const Float4x4& modelToCamera, const Float2& pos, float scale,
                   StringView firstRow, StringView secondRow, const Float4& color) {
    const Assets* assets = Assets::instance;
    TextBuffers scoreTB = generateTextBuffers(assets->sdfFont, secondRow);
    float rectHWid = max(85.f, (scoreTB.width() + 8) * 1.35f);
    drawRoundedRect(assets->texturedShader, modelToCamera, assets->speedLimitTexture.id, color,
                    expand(Rect::fromSize(pos, {0, 0}), Float2{rectHWid, 80} * scale), 32 * scale);
    {
        TextBuffers tb = generateTextBuffers(assets->sdfFont, firstRow);
        drawText(assets->sdfCommon, assets->sdfFont, tb,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({pos.x, pos.y + 32 * scale, 0}) *
                     Float4x4::makeScale(scale * 0.9f) *
                     Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                 {0.75f, 16.f * scale}, {0.f, 0.f, 0.f, 1.f});
    }
    {
        drawText(assets->sdfCommon, assets->sdfFont, scoreTB,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({pos.x, pos.y - 55 * scale, 0}) *
                     Float4x4::makeScale(scale * 2.7f) *
                     Float4x4::makeTranslation({-scoreTB.xMid(), 0, 0}),
                 {0.75f, 64.f * scale}, {0.f, 0.f, 0.f, 1.f});
    }
}

Float3 getNorm(const TitleRotator* rot, float relTime) {
    float t = 1.f;
    if (rot->state == TitleRotator::Tilting) {
        t = clamp((rot->time - relTime) / TitleRotator::TiltTime, 0.f, 1.5f);
        t = applySimpleCubic(t);
    }
    return mix(rot->startNorm, rot->endNorm, t);
}

struct ViewportFrustum {
    Rect viewport;
    Rect frustum;

    ViewportFrustum quantize() const {
        Rect qvp = {quantizeDown(this->viewport.mins + 0.5f, 1.f),
                    quantizeDown(this->viewport.maxs + 0.5f, 1.f)};
        Rect fracs = this->viewport.unmix(qvp);
        return {qvp, this->frustum.mix(fracs)};
    }
};

ViewportFrustum fitFrustumInViewport(const Rect& viewport, const Rect& frustum) {
    PLY_ASSERT(!frustum.isEmpty());
    PLY_ASSERT(!viewport.isEmpty());

    if (frustum.width() * viewport.height() >= frustum.height() * viewport.width()) {
        // Frustum aspect is wider than (or equal to) viewport aspct
        float fitViewportHeight = viewport.width() * frustum.height() / frustum.width();
        float halfExcess = (viewport.height() - fitViewportHeight) * 0.5f;
        return {expand(viewport, {0, -halfExcess}), frustum};
    } else {
        // Frustum aspect is taller than viewport aspect
        float fitViewportWidth = viewport.height() * frustum.width() / frustum.height();
        float halfExcess = (viewport.width() - fitViewportWidth) * 0.5f;
        return {expand(viewport, {-halfExcess, 0}), frustum};
    }
}

Array<Float4x4> composeBirdBones(const GameState* gs) {
    const Assets* a = Assets::instance;

    float wingMix;
    if (gs->wingTime < 1.f) {
        wingMix = applySimpleCubic(gs->wingTime);
    } else {
        wingMix = applySimpleCubic(2.f - gs->wingTime);
    }
    wingMix = clamp(wingMix, 0.f, 1.f);

    Array<Float4x4> deltas;
    deltas.resize(a->bad.birdSkel.numItems());
    for (Float4x4& delta : deltas) {
        delta = Float4x4::identity();
    }

    // Apply wing pose
    for (u32 i = 0; i < a->bad.loWingPose.numItems(); i++) {
        PLY_ASSERT(a->bad.loWingPose[i].boneIndex == a->bad.hiWingPose[i].boneIndex);
        float zAngle = mix(a->bad.loWingPose[i].zAngle, a->bad.hiWingPose[i].zAngle, wingMix);
        deltas[a->bad.loWingPose[i].boneIndex] = Float4x4::makeRotation({0, 0, 1}, zAngle);
    }

    // Apply eye pose
    {
        ArrayView<const PoseBone> from = a->bad.eyePoses[gs->eyePos].view();
        ArrayView<const PoseBone> to = a->bad.eyePoses[(gs->eyePos + 1) % 3].view();
        float f = 0;
        if (gs->eyeMoving) {
            f = applySimpleCubic(gs->eyeTime);
        }
        for (u32 i = 0; i < from.numItems; i++) {
            PLY_ASSERT(from[i].boneIndex == to[i].boneIndex);
            float zAngle = mix(from[i].zAngle, to[i].zAngle, f);
            deltas[from[i].boneIndex] = Float4x4::makeRotation({0, 0, 1}, zAngle);
        }
    }

    Array<Float4x4> curBoneToModel;
    curBoneToModel.resize(a->bad.birdSkel.numItems());
    for (u32 i = 0; i < a->bad.birdSkel.numItems(); i++) {
        const Bone& bone = a->bad.birdSkel[i];
        if (bone.parentIdx >= 0) {
            Float4x4 curBoneToParent = bone.boneToParent * deltas[i];
            curBoneToModel[i] = curBoneToModel[bone.parentIdx] * curBoneToParent;
        } else {
            curBoneToModel[i] = bone.boneToParent * deltas[i];
        }
    }
    return curBoneToModel;
}

void render(GameFlow* gf, const IntVec2& fbSize) {
    GameState* gs = gf->gameState;
    PLY_SET_IN_SCOPE(DynamicArrayBuffers::instance, &gf->dynBuffers);
    gf->dynBuffers.beginFrame();
    float intervalFrac = gf->fracTime / gf->simulationTimeStep;

    // Enable depth test
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));

    // Clear viewport
    GL_CHECK(ClearColor(0, 0, 0, 1));
    GL_CHECK(ClearDepth(1.0));
    GL_CHECK(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    // Fit frustum in viewport
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    float worldDistance = 80.f;
    ViewportFrustum vf = fitFrustumInViewport({{0, 0}, {(float) fbSize.x, (float) fbSize.y}},
                                              visibleExtents / worldDistance)
                             .quantize();
    GL_CHECK(Viewport((GLint) vf.viewport.mins.x, (GLint) vf.viewport.mins.y,
                      (GLsizei) vf.viewport.width(), (GLsizei) vf.viewport.height()));

    // Disable alpha blend
    GL_CHECK(Disable(GL_BLEND));

    // Enable face culling
    GL_CHECK(Enable(GL_CULL_FACE));
    GL_CHECK(CullFace(GL_BACK));
    GL_CHECK(FrontFace(GL_CCW));

#if !PLY_TARGET_IOS // doesn't exist in OpenGLES 3
    GL_CHECK(Enable(GL_FRAMEBUFFER_SRGB));
#endif

    const Assets* a = Assets::instance;
    Float4x4 cameraToViewport =
        Float4x4::makeProjection(visibleExtents / worldDistance, 10.f, 100.f);

    // Draw bird
    Float3 birdRelWorld = mix(gs->birdPos[0], gs->birdPos[1], intervalFrac);
    Float4x4 w2c = {{{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}};
    Float4x4 worldToCamera;
    if (gs->animState == AnimState::Title) {
        float yRise = 0.f;
        if (gs->birdRising) {
            yRise = applySimpleCubic(gs->risingTime);
        } else {
            yRise = 1.f - applySimpleCubic(gs->risingTime);
        }

        Float3 camRelWorld = {
            Complex::fromAngle(mix(gs->birdOrbit[0], gs->birdOrbit[1], gf->fracTime)) * 15, 3.5f};
        Float3x4 cameraToWorld =
            extra::makeBasis(
                (birdRelWorld + Float3{0, 0, 1.3f + mix(-0.15f, 0.15f, yRise)} - camRelWorld)
                    .normalized(),
                {0, 0, 1}, Axis3::ZNeg, Axis3::YPos)
                .toFloat3x4(camRelWorld);
        worldToCamera = cameraToWorld.invertedOrtho().toFloat4x4();
    } else {
        Float3 camRelWorld = {mix(gs->camX[0], gs->camX[1], intervalFrac), -worldDistance, 0};
        worldToCamera = w2c * Float4x4::makeTranslation(-camRelWorld);
    }
    {
        float angle = mix(gs->angle[0], gs->angle[1], intervalFrac);
        float base = 0;
        if (gs->animState != AnimState::Title) {
            base = 0.1f;
        }
        Array<Float4x4> boneToModel = composeBirdBones(gs);
        a->skinnedShader->begin(cameraToViewport);
        a->skinnedShader->draw(
            worldToCamera * Float4x4::makeTranslation(birdRelWorld) *
                Float4x4::makeRotation({0, 1, 0}, -Pi * (angle * gs->flipDirection * 2.f + base)) *
                Float4x4::makeRotation({0, 0, 1}, Pi / 2.f) * Float4x4::makeScale(1.0833f),
            boneToModel.view(), a->bird.view());
    }

    // Draw pipes
    a->matShader->begin(cameraToViewport);
    for (u32 i = 0; i < gs->pipes.numItems(); i++) {
        Float3x4 pipeToWorld = gs->pipes[i];
        a->matShader->draw(worldToCamera * pipeToWorld, a->pipe.view());
    }

    // Draw floor
    a->matShader->draw(worldToCamera *
                           Float4x4::makeTranslation({worldToCamera.invertedOrtho()[3].x, 0.f,
                                                      visibleExtents.mins.y + 4.f}) *
                           Float4x4::makeRotation({0, 0, 1}, Pi / 2.f),
                       a->floor.view());

    // Draw background
    GL_CHECK(DepthMask(GL_FALSE));
    a->flatShader->drawQuad(
        Float4x4::makeTranslation({0, 0, 0.999f}),
        {sRGBToLinear(113.f / 255), sRGBToLinear(200.f / 255), sRGBToLinear(206.f / 255)});

    // Draw flash
    if (gs->animState == AnimState::Impact) {
        a->flashShader->drawQuad(
            cameraToViewport * worldToCamera * Float4x4::makeTranslation(gs->collisionPos) *
                Float4x4::makeRotation({1, 0, 0}, Pi * 0.5f) * Float4x4::makeScale(2.f),
            {0.25f, -0.25f, 0.75f, 0.25f}, a->flashTexture.id, {1.2f, 1.2f, 0, 0.6f});
    }

    if (gs->animState == AnimState::Title) {
        // Draw title
        worldDistance = 15.f;
        Float4x4 cameraToViewport =
            Float4x4::makeProjection(visibleExtents / worldDistance, 1.f, 40.f);
        Float3 skewNorm = getNorm(gf->titleRot, gf->fracTime);
        Float4x4 skewRot = Quaternion::fromUnitVectors(Float3{0, 0, 1}, skewNorm).toFloat4x4();
        Float4x4 mat = cameraToViewport * w2c * Float4x4::makeTranslation({0, worldDistance, 4.f}) *
                       Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) * skewRot *
                       Float4x4::makeTranslation({0, 0, 2.2f}) * Float4x4::makeScale(7.5f);
        GL_CHECK(DepthMask(GL_FALSE));
        a->flatShader->draw(mat, a->outline.view());
        GL_CHECK(DepthMask(GL_TRUE));
        a->flatShader->draw(mat, a->title.view());
        if (gs->showPrompt) {
            TextBuffers tapToPlay = generateTextBuffers(a->sdfFont, "TAP TO PLAY");
            drawText(a->sdfCommon, a->sdfFont, tapToPlay,
                     Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                         Float4x4::makeTranslation({244, 20, 0}) * Float4x4::makeScale(0.9f) *
                         Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                     {0.85f, 1.75f}, {0, 0, 0, 0.4f});
            drawText(a->sdfCommon, a->sdfFont, tapToPlay,
                     Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                         Float4x4::makeTranslation({240, 24, 0}) * Float4x4::makeScale(0.9f) *
                         Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                     {0.75f, 16.f}, {1.f, 1.f, 1.f, 1.f});
        }
    } else if (gs->animState == AnimState::Dead) {
        TextBuffers gameOver = generateTextBuffers(a->sdfFont, "GAME OVER");
        drawText(a->sdfCommon, a->sdfFont, gameOver,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({244, 520, 0}) * Float4x4::makeScale(1.8f) *
                     Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                 {0.85f, 1.75f}, {0, 0, 0, 0.4f});
        drawText(a->sdfCommon, a->sdfFont, gameOver,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({240, 524, 0}) * Float4x4::makeScale(1.8f) *
                     Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                 {0.75f, 32.f}, {1.f, 0.85f, 0.0f, 1.f});
        drawScoreSign(Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f), {240, 380}, 1.f,
                      "SCORE", String::from(gs->score), {1, 1, 1, 1});
        drawScoreSign(Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f), {240, 250}, 0.5f,
                      "BEST", String::from(gf->bestScore), {1.f, 0.45f, 0.05f, 1.f});
        TextBuffers playAgain = generateTextBuffers(a->sdfFont, "TAP TO PLAY AGAIN");
        drawText(a->sdfCommon, a->sdfFont, playAgain,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({244, 20, 0}) * Float4x4::makeScale(0.9f) *
                     Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                 {0.85f, 1.75f}, {0, 0, 0, 0.4f});
        drawText(a->sdfCommon, a->sdfFont, playAgain,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({240, 24, 0}) * Float4x4::makeScale(0.9f) *
                     Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                 {0.75f, 16.f}, {1.f, 1.f, 1.f, 1.f});
    } else {
        // Draw score
        TextBuffers tb = generateTextBuffers(a->sdfFont, String::from(gs->score));
        drawText(a->sdfCommon, a->sdfFont, tb,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({244, 570, 0}) * Float4x4::makeScale(1.5f) *
                     Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                 {0.85f, 1.75f}, {0, 0, 0, 0.4f});
        drawText(a->sdfCommon, a->sdfFont, tb,
                 Float4x4::makeOrtho({{0, 0}, {480, 640}}, -1.f, 1.f) *
                     Float4x4::makeTranslation({240, 574, 0}) * Float4x4::makeScale(1.5f) *
                     Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                 {0.75f, 32.f}, {1.f, 1.f, 1.f, 1.f});
    }
}

} // namespace flap
