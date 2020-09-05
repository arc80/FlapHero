#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>

namespace flap {

struct ViewportFrustum {
    Rect viewport;
    Rect frustum; // 3D
    Rect bounds2D;

    PLY_NO_DISCARD ViewportFrustum quantize() const {
        Rect qvp = {quantizeDown(this->viewport.mins + 0.5f, 1.f),
                    quantizeDown(this->viewport.maxs + 0.5f, 1.f)};
        Rect fracs = this->viewport.unmix(qvp);
        return {qvp, this->frustum.mix(fracs), this->bounds2D.mix(fracs)};
    }

    PLY_NO_DISCARD ViewportFrustum clip(const Rect& clipViewport) const {
        Rect clipped = intersect(this->viewport, clipViewport);
        Rect fracs = this->viewport.unmix(clipped);
        return {clipped, this->frustum.mix(fracs), this->bounds2D.mix(fracs)};
    }
};

ViewportFrustum fitFrustumInViewport(const Rect& viewport, const Rect& frustum,
                                     const Rect& bounds2D) {
    PLY_ASSERT(!frustum.isEmpty());
    PLY_ASSERT(!viewport.isEmpty());

    if (frustum.width() * viewport.height() >= frustum.height() * viewport.width()) {
        // Frustum aspect is wider than (or equal to) viewport aspct
        float fitViewportHeight = viewport.width() * frustum.height() / frustum.width();
        float halfExcess = (viewport.height() - fitViewportHeight) * 0.5f;
        return {expand(viewport, {0, -halfExcess}), frustum, bounds2D};
    } else {
        // Frustum aspect is taller than viewport aspect
        float fitViewportWidth = viewport.height() * frustum.width() / frustum.height();
        float halfExcess = (viewport.width() - fitViewportWidth) * 0.5f;
        return {expand(viewport, {-halfExcess, 0}), frustum, bounds2D};
    }
}

struct DrawContext {
    const GameState* gs = nullptr;
    ViewportFrustum vf;
    ViewportFrustum fullVF;
    float fracTime = 0;
    float intervalFrac = 0;
    Rect visibleExtents = {{0, 0}, {0, 0}};

    static DrawContext* instance_;
    static PLY_INLINE DrawContext* instance() {
        PLY_ASSERT(instance_);
        return instance_;
    };
};

DrawContext* DrawContext::instance_ = nullptr;

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
    addQuad({bounds.mins, bounds.mins + Float2{r, r}}, {{0, 0}, {1, 1}});
    addQuad({bounds.mins + Float2{r, 0}, bounds.bottomRight() + Float2{-r, r}}, {{1, 0}, {1, 1}});
    addQuad({bounds.bottomRight() + Float2{-r, 0}, bounds.bottomRight() + Float2{0, r}},
            {{1, 0}, {0, 1}});

    // Middle row
    addQuad({bounds.mins + Float2{0, r}, bounds.topLeft() + Float2{r, -r}}, {{0, 1}, {1, 1}});
    addQuad({bounds.mins + Float2{r, r}, bounds.maxs + Float2{-r, -r}}, {{1, 1}, {1, 1}});
    addQuad({bounds.bottomRight() + Float2{-r, r}, bounds.maxs + Float2{0, -r}}, {{1, 1}, {0, 1}});

    // Top row
    addQuad({bounds.topLeft() + Float2{0, -r}, bounds.topLeft() + Float2{r, 0}}, {{0, 1}, {1, 0}});
    addQuad({bounds.topLeft() + Float2{r, -r}, bounds.maxs + Float2{-r, 0}}, {{1, 1}, {1, 0}});
    addQuad({bounds.maxs + Float2{-r, -r}, bounds.maxs}, {{1, 1}, {0, 0}});

    shader->draw(modelToViewport, textureID, color, verts.view(), indices.view());
}

void drawScoreSign(const Float4x4& cameraToViewport, const Float2& pos, float scale,
                   StringView firstRow, StringView secondRow, const Float4& color) {
    const Assets* assets = Assets::instance;
    TextBuffers scoreTB = generateTextBuffers(assets->sdfFont, secondRow);
    float rectHWid = max(85.f, (scoreTB.width() + 8) * 1.35f);
    drawRoundedRect(assets->texturedShader, cameraToViewport, assets->speedLimitTexture.id, color,
                    expand(Rect::fromSize(pos, {0, 0}), Float2{rectHWid, 80} * scale), 32 * scale);
    {
        TextBuffers tb = generateTextBuffers(assets->sdfFont, firstRow);
        drawText(assets->sdfCommon, assets->sdfFont, tb,
                 cameraToViewport * Float4x4::makeTranslation({pos.x, pos.y + 32 * scale, 0}) *
                     Float4x4::makeScale(scale * 0.9f) *
                     Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                 {0.75f, 16.f * scale}, {0.f, 0.f, 0.f, 1.f});
    }
    {
        drawText(assets->sdfCommon, assets->sdfFont, scoreTB,
                 cameraToViewport * Float4x4::makeTranslation({pos.x, pos.y - 55 * scale, 0}) *
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

Array<Float4x4> composeBirdBones(const GameState* gs, float intervalFrac) {
    const Assets* a = Assets::instance;

    float wingMix;
    float wingTime = mix(gs->birdAnim.wingTime[0], gs->birdAnim.wingTime[1], intervalFrac);
    if (wingTime < 1.f) {
        wingMix = applySimpleCubic(wingTime);
    } else {
        wingMix = applySimpleCubic(2.f - wingTime);
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
        ArrayView<const PoseBone> from = a->bad.eyePoses[gs->birdAnim.eyePos].view();
        ArrayView<const PoseBone> to = a->bad.eyePoses[(gs->birdAnim.eyePos + 1) % 3].view();
        float f = 0;
        if (gs->birdAnim.eyeMoving) {
            float eyeTime = mix(gs->birdAnim.eyeTime[0], gs->birdAnim.eyeTime[1], intervalFrac);
            f = applySimpleCubic(eyeTime);
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

void Pipe::draw(const Obstacle::DrawParams& params) const {
    const Assets* a = Assets::instance;

    for (const DrawMesh* dm : a->pipe) {
        a->pipeShader->draw(params.cameraToViewport, params.worldToCamera * this->pipeToWorld,
                           Float2{0.035f, 0.035f}, dm, a->pipeEnvTexture.id);
    }
}

void drawTitle(const TitleScreen* titleScreen) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();

    Float4x4 w2c = {{{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}};
    float worldDistance = 15.f;
    Float4x4 cameraToViewport =
        Float4x4::makeProjection(dc->visibleExtents / worldDistance, 1.f, 40.f);
    Float3 skewNorm = getNorm(&titleScreen->titleRot, dc->fracTime);
    Float4x4 skewRot = Quaternion::fromUnitVectors(Float3{0, 0, 1}, skewNorm).toFloat4x4();
    Float4x4 mat = cameraToViewport * w2c * Float4x4::makeTranslation({0, worldDistance, 4.f}) *
                   Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) * skewRot *
                   Float4x4::makeTranslation({0, 0, 2.2f}) * Float4x4::makeScale(7.5f);
    GL_CHECK(DepthRange(0.0, 0.5));
    for (const DrawMesh* dm : a->title) {
        a->flatShader->draw(mat, dm, true);
    }
    GL_CHECK(DepthRange(0.5, 0.5));
    for (const DrawMesh* dm : a->outline) {
        a->flatShader->draw(mat, dm, true);
    }
    for (const DrawMesh* dm : a->blackOutline) {
        a->flatShader->draw(mat, dm, true);
    }
}

void drawStars(const TitleScreen* titleScreen) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();
    const Rect& fullBounds2D = dc->fullVF.bounds2D;

    // Draw stars
    Array<FlatShaderInstanced::InstanceData> insData;
    insData.reserve(titleScreen->starSys.stars.numItems());
    Float4x4 worldToViewport = Float4x4::makeOrtho(
        (fullBounds2D - fullBounds2D.mid()) * (2.f / fullBounds2D.width()), -10.f, 1.01f);
    for (StarSystem::Star& star : titleScreen->starSys.stars) {
        auto& ins = insData.append();
        float angle = mix(star.angle[0], star.angle[1], dc->intervalFrac);
        Float2 pos = mix(star.pos[0], star.pos[1], dc->intervalFrac);
        ins.modelToViewport = worldToViewport * Float4x4::makeTranslation({pos, -star.z}) *
                              Float4x4::makeRotation({0, 0, 1}, angle) * Float4x4::makeScale(0.1f);
        ins.color = {star.color, 1};
    }
    GL_CHECK(DepthRange(0.5, 1.0));
    a->flatShaderInstanced->draw(a->star[0], insData.view());
}

void applyTitleScreen(const DrawContext* dc, float opacity) {
    const Assets* a = Assets::instance;
    const GameState* gs = dc->gs;
    const TitleScreen* ts = gs->titleScreen;

    GL_CHECK(Enable(GL_STENCIL_TEST));
    GL_CHECK(StencilFunc(GL_EQUAL, 0, 0xFF));
    GL_CHECK(StencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    a->copyShader->drawQuad(Float4x4::identity(), ts->tempTex.id, opacity);
    GL_CHECK(Disable(GL_STENCIL_TEST));
}

void renderGamePanel(const DrawContext* dc) {
    const ViewportFrustum& vf = dc->vf;
    const Assets* a = Assets::instance;
    const GameState* gs = dc->gs;

    Float4x4 cameraToViewport = Float4x4::makeProjection(vf.frustum, 10.f, 10000.f);
    GL_CHECK(Viewport((GLint) vf.viewport.mins.x, (GLint) vf.viewport.mins.y,
                      (GLsizei) vf.viewport.width(), (GLsizei) vf.viewport.height()));

    // Draw bird
    Float3 birdRelWorld = mix(gs->bird.pos[0], gs->bird.pos[1], dc->intervalFrac);
    QuatPos camToWorld = {mix(gs->camToWorld[0].quat, gs->camToWorld[1].quat, dc->intervalFrac),
                          mix(gs->camToWorld[0].pos, gs->camToWorld[1].pos, dc->intervalFrac)};
    Float4x4 worldToCamera = camToWorld.inverted().toFloat4x4();
    {
        Quaternion rot = mix(gs->bird.rot[0], gs->bird.rot[1], dc->intervalFrac);
        Array<Float4x4> boneToModel = composeBirdBones(gs, dc->intervalFrac);
        GL_CHECK(Enable(GL_STENCIL_TEST));
        GL_CHECK(StencilFunc(GL_ALWAYS, 1, 0xFF));
        GL_CHECK(StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
        GL_CHECK(StencilMask(0xFF));
        for (const DrawMesh* dm : a->bird) {
            a->skinnedShader->draw(
                cameraToViewport,
                worldToCamera * Float4x4::makeTranslation(birdRelWorld) * rot.toFloat4x4() *
                    Float4x4::makeRotation({0, 0, 1}, Pi / 2.f) * Float4x4::makeScale(1.0833f),
                boneToModel.view(), dm);
        }
        GL_CHECK(Disable(GL_STENCIL_TEST));
    }

    if (!gs->mode.title()) {
        // Draw obstacles
        Obstacle::DrawParams odp;
        odp.cameraToViewport = cameraToViewport;
        odp.worldToCamera = worldToCamera;
        for (const Obstacle* obst : gs->playfield.obstacles) {
            obst->draw(odp);
        }

        // Draw floor
        for (const DrawMesh* dm : a->floorStripe) {
            a->texMatShader->draw(
                cameraToViewport,
                worldToCamera *
                    Float4x4::makeTranslation({0.f, 0.f, dc->visibleExtents.mins.y + 4.f}) *
                    Float4x4::makeRotation({0, 0, 1}, Pi / 2.f),
                dm, a->stripeTexture.id);
        }
        for (const DrawMesh* dm : a->floor) {
            a->matShader->draw(
                cameraToViewport,
                worldToCamera *
                    Float4x4::makeTranslation({0.f, 0.f, dc->visibleExtents.mins.y + 4.f}) *
                    Float4x4::makeRotation({0, 0, 1}, Pi / 2.f),
                dm);
        }

        // Draw shrubs
        Float3 skyColor = fromSRGB(Float3{113.f / 255, 200.f / 255, 206.f / 255});
        {
            ShrubShader::Props shrubProps;
            shrubProps.diffuse[0] = mix(fromSRGB(Float3{0.2f, 0.7f, 0.f} * 0.85f), skyColor, 0.25f);
            shrubProps.diffuse[1] = mix(fromSRGB(Float3{0.3f, 0.85f, 0.3f}), skyColor, 0.15f);
            shrubProps.specular = {mix(Float3{1, 1, 1}, skyColor, 0.25f), 0.15f};
            shrubProps.specPower = 1.f;
            shrubProps.shade = {mix(Float3{0, 0.2f, 0.4f}, skyColor, 0.2f), 0.6f};
            shrubProps.rim = {mix(Float3{1, 1, 1}, skyColor, 0.5f), 0.5f};
            for (u32 i = 0; i < 3; i++) {
                for (const DrawGroup::Instance& inst : a->shrubGroup.instances) {
                    GLuint texID =
                        (inst.drawMesh == a->shrub[0]) ? a->shrubTexture.id : a->shrub2Texture.id;
                    a->shrubShader->draw(cameraToViewport,
                                         worldToCamera * a->shrubGroup.groupToWorld *
                                             Float4x4::makeTranslation(
                                                 {gs->shrubX + GameState::ShrubRepeat * i, 0, 0}) *
                                             inst.itemToGroup,
                                         inst.drawMesh, texID, &shrubProps);
                }
            }
        }

        // Draw cities
        Float4x4 skyBoxW2C = worldToCamera;
        skyBoxW2C[3].asFloat2() = {0, 0};
        {
            MaterialShader::Props matProps;
            matProps.specular = 0.0025f;
            matProps.fog = {mix(Float3{1, 1, 1}, skyColor, 0.4f), 0.5f};
            for (float r = -3; r <= 3; r++) {
                for (const DrawGroup::Instance& inst : a->cityGroup.instances) {
                    a->texMatShader->draw(
                        cameraToViewport,
                        worldToCamera * a->cityGroup.groupToWorld *
                            Float4x4::makeTranslation(
                                {gs->buildingX + GameState::BuildingRepeat * r, 0, 0}) *
                            inst.itemToGroup,
                        inst.drawMesh, a->windowTexture.id, &matProps);
                }
            }
        }

        // Draw sky
        a->flatShader->drawQuad(Float4x4::makeTranslation({0, 0, 0.999f}), skyColor);

        // Draw clouds
        float cloudAngle =
            gs->cloudAngleOffset + camToWorld.pos.x * GameState::CloudRadiansPerCameraX;
        for (const DrawMesh* dm : a->cloud) {
            a->texturedShader->draw(cameraToViewport * skyBoxW2C *
                                        Float4x4::makeRotation({0, 0, 1}, cloudAngle),
                                    a->cloudTexture.id, {1, 1, 1, 1}, dm, true);
        }

        // Draw flash
        if (auto impact = gs->mode.impact()) {
            a->flashShader->drawQuad(
                cameraToViewport * worldToCamera * Float4x4::makeTranslation(impact->hit.pos) *
                    Float4x4::makeRotation({1, 0, 0}, Pi * 0.5f) * Float4x4::makeScale(2.f),
                {0.25f, 0.25f, 0.75f, 0.75f}, a->flashTexture.id, {1.2f, 1.2f, 0, 0.6f});
        }

        if (auto dead = gs->mode.dead()) {
            TextBuffers gameOver = generateTextBuffers(a->sdfFont, "GAME OVER");
            drawText(a->sdfCommon, a->sdfFont, gameOver,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({244, 520, 0}) * Float4x4::makeScale(1.8f) *
                         Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                     {0.85f, 1.75f}, {0, 0, 0, 0.4f});
            drawText(a->sdfCommon, a->sdfFont, gameOver,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({240, 524, 0}) * Float4x4::makeScale(1.8f) *
                         Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                     {0.75f, 32.f}, {1.f, 0.85f, 0.0f, 1.f});
            drawScoreSign(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f), {240, 380}, 1.f, "SCORE",
                          String::from(gs->score), {1, 1, 1, 1});
            drawScoreSign(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f), {240, 250}, 0.5f, "BEST",
                          String::from(gs->outerCtx->bestScore), {1.f, 0.45f, 0.05f, 1.f});

            if (dead->showPrompt) {
                TextBuffers playAgain = generateTextBuffers(a->sdfFont, "TAP TO PLAY AGAIN");
                drawText(a->sdfCommon, a->sdfFont, playAgain,
                         Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                             Float4x4::makeTranslation({244, 20, 0}) * Float4x4::makeScale(0.9f) *
                             Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                         {0.85f, 1.75f}, {0, 0, 0, 0.4f});
                drawText(a->sdfCommon, a->sdfFont, playAgain,
                         Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                             Float4x4::makeTranslation({240, 24, 0}) * Float4x4::makeScale(0.9f) *
                             Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                         {0.75f, 16.f}, {1.f, 1.f, 1.f, 1.f});
            }
        }

        if (!gs->mode.dead() && !gs->mode.title()) {
            // Draw score
            TextBuffers tb = generateTextBuffers(a->sdfFont, String::from(gs->score));
            drawText(a->sdfCommon, a->sdfFont, tb,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({244, 570, 0}) * Float4x4::makeScale(1.5f) *
                         Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                     {0.85f, 1.75f}, {0, 0, 0, 0.4f});
            drawText(a->sdfCommon, a->sdfFont, tb,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({240, 574, 0}) * Float4x4::makeScale(1.5f) *
                         Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                     {0.75f, 32.f}, {1.f, 1.f, 1.f, 1.f});
        }

        if (auto trans = gs->camera.transition()) {
            applyTitleScreen(dc, applySimpleCubic(clamp(1.f - trans->param * 2.f, 0.f, 1.f)));
        }
    } else {
        applyTitleScreen(dc, 1.f);
    }
}

void drawTitleScreenToTemp(TitleScreen* ts) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();

    Float2 vpSize = dc->fullVF.viewport.size();
    if (!ts->tempTex.id || ts->tempTex.dims() != vpSize) {
        // Create temporary buffer
        ts->tempRTT.destroy();
        ts->tempTex.destroy();
        SamplerParams params;
        params.minFilter = false;
        params.magFilter = false;
        params.repeatX = false;
        params.repeatY = false;
        params.sRGB = false;
        PLY_ASSERT(isQuantized(vpSize, 1.f));
        ts->tempTex.init((u32) vpSize.x, (u32) vpSize.y, image::Format::RGBA, 1, params);
        ts->tempRTT.init(ts->tempTex, true);
    }

    // Render to it
    GLint prevFBO;
    GL_CHECK(GetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO));
    GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, ts->tempRTT.fboID));
    GL_CHECK(Viewport(0, 0, (u32) vpSize.x, (u32) vpSize.y));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(ClearColor(0.75f, 0.75f, 0.75f, 1.f));
    GL_CHECK(ClearDepth(1.0));
    GL_CHECK(ClearStencil(0));
    GL_CHECK(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    drawTitle(ts);
    drawStars(ts);
    {
        // Draw background
        float hypnoAngle = mix(ts->hypnoAngle[0], ts->hypnoAngle[1], dc->intervalFrac);
        float hypnoScale = powf(1.3f, mix(ts->hypnoZoom[0], ts->hypnoZoom[1], dc->intervalFrac));
        a->hypnoShader->draw(Float4x4::makeOrtho({{-3.f, -4.f}, {3.f, 4.f}}, -1.f, 0.01f) *
                                 Float4x4::makeTranslation({0, -1.7f, 0}) *
                                 Float4x4::makeRotation({0, 0, 1}, hypnoAngle) *
                                 Float4x4::makeScale(hypnoScale),
                             a->waveTexture.id, a->hypnoPaletteTexture, hypnoScale);
    }
    // Draw prompt
    if (ts->showPrompt) {
        TextBuffers tapToPlay = generateTextBuffers(a->sdfFont, "TAP TO PLAY");
        drawText(a->sdfCommon, a->sdfFont, tapToPlay,
                 Float4x4::makeOrtho({{0, 0}, vpSize}, -1.f, 1.f) *
                     Float4x4::makeTranslation({244, 20, 0}) * Float4x4::makeScale(0.9f) *
                     Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                 {0.85f, 1.75f}, {0, 0, 0, 0.8f});
        drawOutlinedText(a->sdfOutline, a->sdfFont, tapToPlay,
                         Float4x4::makeOrtho({{0, 0}, vpSize}, -1.f, 1.f) *
                             Float4x4::makeTranslation({240, 24, 0}) * Float4x4::makeScale(0.9f) *
                             Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                         {1, 1, 1, 0}, {0, 0, 0, 0}, {{0.6f, 16.f}, {0.75f, 12.f}});
    }

    GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, prevFBO));
}

void render(GameFlow* gf, const IntVec2& fbSize) {
    PLY_ASSERT(fbSize.x > 0 && fbSize.y > 0);
    const Assets* a = Assets::instance;
    PLY_SET_IN_SCOPE(DynamicArrayBuffers::instance, &gf->dynBuffers);
    gf->dynBuffers.beginFrame();
    float intervalFrac = gf->fracTime / gf->simulationTimeStep;

#if !PLY_TARGET_IOS && !PLY_TARGET_ANDROID// doesn't exist in OpenGLES 3
    GL_CHECK(Enable(GL_FRAMEBUFFER_SRGB));
#endif

    // Enable face culling
    GL_CHECK(Enable(GL_CULL_FACE));
    GL_CHECK(CullFace(GL_BACK));
    GL_CHECK(FrontFace(GL_CCW));

    // Fit frustum in viewport
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    ViewportFrustum fullVF =
        fitFrustumInViewport({{0, 0}, {(float) fbSize.x, (float) fbSize.y}},
                             visibleExtents / GameState::WorldDistance, Rect{{0, 0}, {480, 640}})
            .quantize();

    // Before drawing the panels, draw the title screen (if any) to a temporary buffer
    if (gf->gameState->titleScreen) {
        DrawContext dc;
        PLY_SET_IN_SCOPE(DrawContext::instance_, &dc);
        dc.gs = gf->gameState;
        dc.vf = fullVF;
        dc.fullVF = fullVF;
        dc.fracTime = gf->fracTime;
        dc.intervalFrac = intervalFrac;
        dc.visibleExtents = visibleExtents;
        drawTitleScreenToTemp(gf->gameState->titleScreen);
    }

    // Clear viewport
    GL_CHECK(Viewport(0, 0, fbSize.x, fbSize.y));
    GL_CHECK(DepthRange(0.0, 1.0));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(ClearColor(0, 0, 0, 1));
    GL_CHECK(ClearDepth(1.0));
    GL_CHECK(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    // Screen wipe transition
    auto renderPanel = [&](const GameState* gs, const ViewportFrustum& vf) {
        DrawContext dc;
        PLY_SET_IN_SCOPE(DrawContext::instance_, &dc);
        dc.gs = gs;
        dc.vf = vf;
        dc.fullVF = fullVF;
        dc.fracTime = gf->fracTime;
        dc.intervalFrac = intervalFrac;
        dc.visibleExtents = visibleExtents;
        renderGamePanel(&dc);
    };

    if (auto trans = gf->trans.on()) {
        // Apply slide motion curve
        float slide = mix(trans->frac[0], trans->frac[1], intervalFrac);
        slide = interpolateCubic<float>(0, 0.1f, 0.9f, 1, slide);
        slide = interpolateCubic<float>(0, 0.1f, 0.9f, 1, slide);

        // Place divider
        float divWidth = quantizeNearest(max(1.f, fullVF.viewport.width() / 60.f), 1.f);
        float divRight = fullVF.viewport.mins.x + (fullVF.viewport.width() + divWidth) * slide;
        float divLeft = divRight - divWidth;

        // Draw left (new) panel
        {
            ViewportFrustum leftVF = fullVF;
            leftVF.viewport.mins.x = divLeft - fullVF.viewport.width();
            leftVF.viewport.maxs.x = divLeft;
            leftVF = leftVF.clip(fullVF.viewport).quantize();

            if (!leftVF.viewport.isEmpty()) {
                renderPanel(gf->gameState, leftVF);
            }
        }

        // Draw vertical bar
        {
            Rect barRect = quantizeNearest(
                intersect(fullVF.viewport, Rect{{divLeft, fullVF.viewport.mins.y},
                                                {divRight, fullVF.viewport.maxs.y}}),
                1.f);
            if (!barRect.isEmpty()) {
                GL_CHECK(Viewport((GLint) barRect.mins.x, (GLint) barRect.mins.y,
                                  (GLsizei) barRect.width(), (GLsizei) barRect.height()));
                a->flatShader->drawQuad(Float4x4::identity(), {1, 1, 1});
            }
        }

        // Draw right (old) panel
        {
            ViewportFrustum rightVF = fullVF;
            rightVF.viewport.mins.x = divRight;
            rightVF.viewport.maxs.x = divRight + fullVF.viewport.width();
            rightVF = rightVF.clip(fullVF.viewport).quantize();
            if (!rightVF.viewport.isEmpty()) {
                PLY_ASSERT(!trans->oldGameState->mode.title());
                renderPanel(trans->oldGameState, rightVF);
            }
        }
    } else {
        renderPanel(gf->gameState, fullVF);
    }
}

} // namespace flap
