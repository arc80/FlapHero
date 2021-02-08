#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>
#include <flapGame/DrawContext.h>

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

    shader->draw(modelToViewport, textureID, color, verts, indices, true);
}

void drawScoreSign(const Float4x4& cameraToViewport, const Float2& pos, float scale,
                   StringView firstRow, StringView secondRow, const Float4& color) {
    const Assets* assets = Assets::instance;
    TextBuffers scoreTB = generateTextBuffers(assets->sdfFont, secondRow);
    {
        TextBuffers tb = generateTextBuffers(assets->sdfFont, firstRow);
        drawText(assets->sdfCommon, assets->sdfFont, tb,
                 cameraToViewport * Float4x4::makeTranslation({pos.x, pos.y + 32 * scale, 0}) *
                     Float4x4::makeScale(scale * 0.9f) *
                     Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                 {0.75f, 16.f * scale}, {0.f, 0.f, 0.f, 0.98f}, true);
    }
    {
        drawText(assets->sdfCommon, assets->sdfFont, scoreTB,
                 cameraToViewport * Float4x4::makeTranslation({pos.x, pos.y - 55 * scale, 0}) *
                     Float4x4::makeScale(scale * 2.7f) *
                     Float4x4::makeTranslation({-scoreTB.xMid(), 0, 0}),
                 {0.75f, 64.f * scale}, {0.f, 0.f, 0.f, 0.98f}, true);
    }
    {
        float rectHWid = max(85.f, (scoreTB.width() + 8) * 1.35f);
        drawRoundedRect(assets->texturedShader, cameraToViewport, assets->speedLimitTexture.id,
                        color, expand(Rect::fromSize(pos, {0, 0}), Float2{rectHWid, 80} * scale),
                        32 * scale);
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

Array<QuatPos> tonguePtsToXforms(ArrayView<const Float3> pts) {
    const Assets* a = Assets::instance;

    Array<QuatPos> result;
    result.resize(pts.numItems);
    result[0] = QuatPos::fromOrtho(a->bad.birdSkel[a->bad.tongueBones[0].boneIndex].boneToParent);
    for (u32 i = 1; i < pts.numItems; i++) {
        const Quaternion& prevRot = result[i - 1].quat;
        Float3 prevDir = prevRot.rotateUnitY();
        Float3 dir = (pts[i] - pts[i - 1]).safeNormalized(prevDir);
        Quaternion rot = (Quaternion::fromUnitVectors(prevDir, dir) * prevRot).normalized();
        result[i] = QuatPos{rot, pts[i]};
        result[i].pos = result[i] * Float3{0, -a->bad.tongueBones[i].length * 0.5f, 0};
    }
    return result;
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
        ArrayView<const PoseBone> from = a->bad.eyePoses[gs->birdAnim.eyePos[0]];
        ArrayView<const PoseBone> to = a->bad.eyePoses[gs->birdAnim.eyePos[1]];
        float f = 1;
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

    // Compute boneToModel xforms
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

    // Apply tongue
    {
        Quaternion worldToBirdRot = Quaternion::fromAxisAngle({0, 0, 1}, -Pi / 2.f) *
                                    mix(gs->bird.finalRot[0], gs->bird.finalRot[1], 1.f).inverted();
        Array<Float3> tonguePts;
        tonguePts.resize(a->bad.tongueBones.numItems());
        const Tongue::State& prevState = gs->bird.tongue.states[1 - gs->bird.tongue.curIndex];
        const Tongue::State& curState = gs->bird.tongue.states[gs->bird.tongue.curIndex];
        float f = gs->bird.tongue.isPaused ? 1.f : intervalFrac;
        for (u32 i = 0; i < tonguePts.numItems(); i++) {
            tonguePts[i] = worldToBirdRot * mix(prevState.pts[i], curState.pts[i], f);
        }
        Array<QuatPos> tongueXforms = tonguePtsToXforms(tonguePts);
        for (u32 i = 0; i < tonguePts.numItems(); i++) {
            u32 bi = a->bad.tongueBones[i].boneIndex;
            curBoneToModel[bi] =
                Float4x4::fromQuatPos(tongueXforms[i]) * Float4x4::makeScale({1.f, 0.5f, 1.f});
        }
    }

    return curBoneToModel;
}

void Pipe::draw(const Obstacle::DrawParams& params) const {
    const Assets* a = Assets::instance;

    for (const DrawMesh* dm : a->pipe) {
        a->pipeShader->draw(params.cameraToViewport, params.worldToCamera * this->pipeToWorld,
                            Float2{0.035f, 0.025f}, dm, a->pipeEnvTexture.id);
    }
}

void drawTitle(const TitleScreen* titleScreen, const Float4x4& extraZoom) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();

    Float4x4 w2c = {{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}};
    float worldDistance = 15.f;
    float offset = 0.25f;
    float frustumScale = mix(0.15f, 0.18f, dc->fullVF.bounds2D.mins.y / -200.f);

    Float4x4 cameraToViewport =
        extraZoom *
        Float4x4::makeProjection(dc->fullVF.frustum / frustumScale - Float2{0, offset}, 1.f, 40.f);
    Float3 skewNorm = getNorm(&titleScreen->titleRot, dc->fracTime);
    Float4x4 skewRot =
        Float4x4::fromQuaternion(Quaternion::fromUnitVectors(Float3{0, 0, 1}, skewNorm));
    Float4x4 mat = cameraToViewport * w2c * Float4x4::makeTranslation({0, worldDistance, 4.f}) *
                   Float4x4::makeRotation({1, 0, 0}, Pi / 2.f) * skewRot *
                   Float4x4::makeTranslation({0, 0, 3.2f}) * Float4x4::makeScale(7.f);
    GL_CHECK(DepthRange(0.0, 0.5));
    for (const DrawMesh* dm : a->title) {
        a->flatShader->draw(mat, dm, true);
    }
    for (const DrawMesh* dm : a->titleSideBlue) {
        Float3 linear = toSRGB(dm->diffuse);
        a->gradientShader->draw(mat, dm, {mix(linear, {0.1f, 1.f, 1.f}, 0.1f), 1.f},
                                {linear * 0.85f, 1.f});
    }
    for (const DrawMesh* dm : a->titleSideRed) {
        Float3 linear = toSRGB(dm->diffuse);
        a->gradientShader->draw(mat, dm, {mix(linear, {1.f, 0.8f, 0.1f}, 0.08f), 1.f},
                                {linear * 0.8f, 1.f});
    }
    GL_CHECK(DepthRange(0.5, 0.5));
    for (const DrawMesh* dm : a->outline) {
        a->flatShader->draw(mat, dm, true);
    }
    for (const DrawMesh* dm : a->blackOutline) {
        a->flatShader->draw(mat, dm, true);
    }
}

void drawStars(const TitleScreen* titleScreen, const Float4x4& extraZoom) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();
    const Rect& fullBounds2D = dc->fullVF.bounds2D;

    // Draw stars
    Array<StarShader::InstanceData> insData;
    insData.reserve(titleScreen->starSys.stars.numItems());
    float lensDist = 2.f;
    float offset = dc->fullVF.bounds2D.mins.y * (0.12f / -200.f);
    Float4x4 worldToViewport =
        extraZoom *
        Float4x4::makeProjection((fullBounds2D - fullBounds2D.mid()) *
                                         (2.f / (lensDist * fullBounds2D.width())) -
                                     Float2{0, offset},
                                 0.01f, 10.f) *
        Float4x4::makeTranslation({0, 0, -lensDist});
    for (StarSystem::Star& star : titleScreen->starSys.stars) {
        auto& ins = insData.append();
        float life = mix(star.life[0], star.life[1], dc->intervalFrac);
        float angle = mix(star.angle[0], star.angle[1], dc->intervalFrac);
        Float3 pos = mix(star.pos[0], star.pos[1], dc->intervalFrac);
        float scale = clamp(life * 1.f + 0.1f, 0.f, 1.f);
        float alpha = 1.f - clamp((life - 1.5f) * 1.f, 0.f, 1.f);
        ins.modelToViewport = worldToViewport * Float4x4::makeTranslation(pos) *
                              Float4x4::makeRotation({0, 0, 1}, angle) *
                              Float4x4::makeScale(scale * 0.13f);
        ins.color = mix(Float4{1.5f, 1.5f, 1.5f, 1.f}, star.color, min(1.f, life * 1.5f));
        ins.color.a() *= alpha;
    }
    GL_CHECK(DepthRange(0.5, 1.0));
    a->starShader->draw(a->star[0], a->starTexture.id, insData);
}

void applyTitleScreen(const DrawContext* dc, float opacity, float premul) {
    const Assets* a = Assets::instance;
    const GameState* gs = dc->gs;
    const TitleScreen* ts = gs->titleScreen;

    GL_CHECK(Enable(GL_STENCIL_TEST));
    GL_CHECK(StencilFunc(GL_EQUAL, 0, 0xFF));
    GL_CHECK(StencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    Rect orthoFrustum = dc->fullVF.bounds2D.unmix(dc->vf.bounds2D) * 2.f - Float2{1.f};
    a->copyShader->drawQuad(Float4x4::makeOrtho(orthoFrustum, -1.f, 1.f), ts->tempTex.id, opacity,
                            premul);
    GL_CHECK(Disable(GL_STENCIL_TEST));
}

Tuple<float, float> getSignParams(float t) {
    float t1 = 0.18f;
    float t2 = 0.28f;

    float s = 0;
    if (t < t1) {
        s = mix(0.8f, -0.13f, t / t1);
    } else if (t < t2) {
        s = mix(-0.13f, 0.f, unmix(t1, t2, t));
    }
    float a = 1.f;
    if (t < t1) {
        a = powf(t / 0.3f, 2.f);
    }
    return {s, a};
}

void renderGamePanel(const DrawContext* dc) {
    const ViewportFrustum& vf = dc->vf;
    const Assets* a = Assets::instance;
    const GameState* gs = dc->gs;

    Float3 skyColor = fromSRGB(Float3{80 / 255.f, 203 / 255.f, 1});
    float frustumScale = 1.f;
    if (auto dead = gs->lifeState.dead()) {
        frustumScale = min(frustumScale,
                           powf(1.2f, getSignParams(dead->animateSignTime + dc->fracTime).first));
        frustumScale =
            min(frustumScale,
                powf(1.2f, getSignParams(dead->animateSignTime + dc->fracTime - 0.25f).first));
    }
    Float4x4 cameraToViewport = Float4x4::makeProjection(vf.frustum * frustumScale, 10.f, 500.f);
    GL_CHECK(Viewport((GLint) vf.viewport.mins.x, (GLint) vf.viewport.mins.y,
                      (GLsizei) vf.viewport.width(), (GLsizei) vf.viewport.height()));

    // Draw bird
    Float3 birdRelWorld = mix(gs->bird.pos[0], gs->bird.pos[1], dc->intervalFrac);
    QuatPos camToWorld = {mix(gs->camToWorld[0].quat, gs->camToWorld[1].quat, dc->intervalFrac),
                          mix(gs->camToWorld[0].pos, gs->camToWorld[1].pos, dc->intervalFrac)};
    Float4x4 worldToCamera = Float4x4::fromQuatPos(camToWorld.inverted());
    {
        Quaternion birdRot = mix(gs->bird.finalRot[0], gs->bird.finalRot[1], dc->intervalFrac);
        Array<Float4x4> boneToModel = composeBirdBones(gs, dc->intervalFrac);
        GL_CHECK(Enable(GL_STENCIL_TEST));
        GL_CHECK(StencilFunc(GL_ALWAYS, 1, 0xFF));
        GL_CHECK(StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
        GL_CHECK(StencilMask(0xFF));
        Float4x4 modelToCamera = worldToCamera * Float4x4::makeTranslation(birdRelWorld) *
                                 Float4x4::fromQuaternion(birdRot) *
                                 Float4x4::makeRotation({0, 0, 1}, Pi / 2.f) *
                                 Float4x4::makeScale(1.0833f);
        if (gs->isWeak()) {
            auto desaturate = [](const Float3 color, float amt) -> Float3 {
                Float3 gray = Float3{dot(color, Float3{0.333f})};
                return mix(color, gray, amt);
            };
            a->sickBirdMeshes[2]->matProps.diffuse =
                desaturate({0.18f, 0.14f, 0.105f}, -0.9f) * 1.5f;

            for (const Assets::MeshWithMaterial* mm : a->sickBirdMeshes) {
                a->skinnedShader->draw(cameraToViewport, modelToCamera, &mm->mesh,
                                       boneToModel, &mm->matProps);
            }
        } else {
            for (const Assets::MeshWithMaterial* mm : a->birdMeshes) {
                a->skinnedShader->draw(cameraToViewport, modelToCamera, &mm->mesh,
                                       boneToModel, &mm->matProps);
            }
            for (const DrawMesh* dm : a->eyeWhite) {
                a->pipeShader->draw(cameraToViewport, modelToCamera, {0, 0}, dm,
                                    a->eyeWhiteTexture.id);
            }
        }
        GL_CHECK(Disable(GL_STENCIL_TEST));
    }

    if (!gs->mode.title()) {
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
        for (const DrawMesh* dm : a->dirt) {
            UberShader::Props props;
            props.diffuse = {1.1f, 0.9f, 0.2f};
            props.diffuse2 = {0.08f, 0.02f, 0};
            props.texID = a->gradientTexture.id;
            a->duotoneShader->draw(
                cameraToViewport,
                worldToCamera *
                    Float4x4::makeTranslation({0.f, 0.f, dc->visibleExtents.mins.y + 4.f}) *
                    Float4x4::makeRotation({0, 0, 1}, Pi / 2.f),
                dm, {}, &props);
        }

        // Draw obstacles
        Obstacle::DrawParams odp;
        odp.cameraToViewport = cameraToViewport;
        odp.worldToCamera = worldToCamera;
        for (const Obstacle* obst : gs->playfield.obstacles) {
            obst->draw(odp);
        }

        // Draw shrubs
        {
            UberShader::Props props;
            props.lightDir = Float3{1, -1, 0}.normalized();
            props.diffuse = mix(Float3{0.065f, 0.99f, 0.1f} * 1.f, skyColor, 0.04f);
            props.diffuse2 = mix(Float3{0.065f, 0.99f, 0.1f} * 0.5f, skyColor, 0.05f);
            props.diffuseClamp = {0.28f, 1.1f, 0.28f};
            props.specLightDir = Float3{1, -1, 0.2f}.normalized();
            props.specular = Float3{0.7f, 1, 0} * 0.07f;
            props.specPower = 4.f;
            props.rim = {mix(Float3{1, 1, 1}, skyColor, 0.3f) * 0.1f, 1.f};
            props.rimFactor = {1.6f, 4.5f};
            float shrubX = mix(gs->shrubX[0], gs->shrubX[1], dc->intervalFrac);
            for (s32 i = -1; i <= 1; i++) {
                Float3 groupPos = a->shrubGroup.groupRelWorld;
                groupPos.x += shrubX + (GameState::ShrubRepeat * i) * a->shrubGroup.groupScale;
                for (const DrawGroup::Instance& inst : a->shrubGroup.instances) {
                    props.texID =
                        (inst.drawMesh == a->shrub[0]) ? a->shrubTexture.id : a->shrub2Texture.id;
                    a->duotoneShader->draw(cameraToViewport,
                                           worldToCamera * Float4x4::makeTranslation(groupPos) *
                                               Float4x4::makeScale(a->shrubGroup.groupScale) *
                                               inst.itemToGroup,
                                           inst.drawMesh, {}, &props);
                }
            }
        }

        // Draw cities
        Float4x4 skyBoxW2C = worldToCamera;
        skyBoxW2C[3].x = 0;
        skyBoxW2C[3].y = 0;
        {
            UberShader::Props props;
            props.diffuse = Float3{0.95f, 1.15f, 0.35f} * 2.2f;
            props.diffuse2 = Float3{0.2f, 1.f, 1.f} * 2.f;
            props.diffuseClamp = {-1.f, 1.f, 0.4f};
            props.rim = {Float3{0.f, 1.f, 1.f} * 0.7f, 0.3f};
            props.rimFactor = {2.f, 2.f};
            props.specular = {0, 0, 0};
            props.texID = a->windowTexture.id;
            float buildingX = mix(gs->buildingX[0], gs->buildingX[1], dc->intervalFrac);
            for (float r = -3; r <= 3; r++) {
                Float3 groupPos = a->cityGroup.groupRelWorld;
                groupPos.x += buildingX + (GameState::BuildingRepeat * r) * a->cityGroup.groupScale;
                for (const DrawGroup::Instance& inst : a->cityGroup.instances) {
                    a->duotoneShader->draw(cameraToViewport,
                                           worldToCamera * Float4x4::makeTranslation(groupPos) *
                                               Float4x4::makeScale(a->cityGroup.groupScale) *
                                               inst.itemToGroup,
                                           inst.drawMesh, {}, &props);
                }
            }
        }

        // Draw sky
        a->flatShader->drawQuad(Float4x4::makeTranslation({0, 0, 0.999f}), {skyColor, 1.f});

        // Draw clouds
        float cloudAngle =
            gs->cloudAngleOffset + camToWorld.pos.x * GameState::CloudRadiansPerCameraX;
        for (const DrawMesh* dm : a->cloud) {
            a->texturedShader->draw(
                Float4x4::makeProjection(vf.frustum * frustumScale * 2.0f, 10.f, 500.f) *
                    skyBoxW2C * Float4x4::makeRotation({0, 0, 1}, cloudAngle),
                a->cloudTexture.id, {1, 1, 1, 1}, dm, true);
        }

        // Draw puffs
        {
            Array<PuffShader::InstanceData> instances;
            for (const Puffs* puffs : gs->puffs) {
                puffs->addInstances(instances);
            }
            a->puffShader->draw(cameraToViewport * worldToCamera, a->puffNormalTexture.id,
                                instances);
        }

        // Draw bird sweat
        {
            Array<StarShader::InstanceData> insData;
            Float4x4 birdToViewport =
                cameraToViewport * worldToCamera * Float4x4::makeTranslation(birdRelWorld);
            gs->sweat.addInstances(birdToViewport, insData);
            if (!insData.isEmpty()) {
                a->starShader->draw(a->quad, a->sweatTexture.id, insData);
            }
        }

        // Draw impact flash
        if (auto impact = gs->mode.impact()) {
            // Draw full-screen flash
            float t = clamp(impact->time + dc->fracTime, 0.f, 1.f);
            a->flatShader->drawQuad(Float4x4::identity(),
                                    Float4{1, 1, 1, (1.f - t) * (1.f - t) * 0.8f}, false);

            u32 fm = (u32) roundDown(impact->flashFrame);
            a->flashShader->drawQuad(
                cameraToViewport * worldToCamera * Float4x4::makeTranslation(impact->hit.pos) *
                    Float4x4::makeRotation({1, 0, 0}, Pi * 0.5f) * Float4x4::makeScale(1.7f),
                {0.25f, 0.25f, 0.25f + 0.5f * (fm & 1), 0.75f - 0.25f * (fm & 2)},
                a->flashTexture.id, {1.2f, 1.2f, 0, 0.8f});
            // Advance frame
            float nextFrame = impact->flashFrame + dc->renderDT * 60.f;
            if ((u32) roundDown(nextFrame) >= fm + 2) {
                nextFrame = (float) fm + 1.f;
            }
            impact->flashFrame = wrap(nextFrame, 4.f);
        }

        // Draw front clouds
        float frontCloudX = mix(gs->frontCloudX[0], gs->frontCloudX[1], dc->intervalFrac);
        for (const DrawMesh* dm : a->frontCloud) {
            a->texturedShader->draw(cameraToViewport * worldToCamera *
                                        Float4x4::makeTranslation({frontCloudX, -4.f, 21.f}),
                                    a->frontCloudTexture.id, {1, 1, 1, 1}, dm, true);
        }

        // Draw text overlays
        auto dead = gs->lifeState.dead();
        bool showGameOver = dead && dead->delay <= 0;
        if (showGameOver) {
            TextBuffers gameOver = generateTextBuffers(a->sdfFont, "GAME OVER");
            drawText(a->sdfCommon, a->sdfFont, gameOver,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({244, 492, 0}) * Float4x4::makeScale(1.8f) *
                         Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                     {0.85f, 1.75f}, {0, 0, 0, 0.4f});
            drawText(a->sdfCommon, a->sdfFont, gameOver,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({240, 496, 0}) * Float4x4::makeScale(1.8f) *
                         Float4x4::makeTranslation({-gameOver.xMid(), 0, 0}),
                     {0.75f, 32.f}, {1.f, 0.85f, 0.0f, 1.f});

            Tuple<float, float> sp = getSignParams(dead->animateSignTime + dc->fracTime);
            Tuple<float, float> sp2 =
                getSignParams(max(0.f, dead->animateSignTime + dc->fracTime - 0.25f));
            drawScoreSign(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f), {240, 380},
                          powf(2.f, sp.first), "SCORE", String::from(gs->score),
                          {1, 1, 1, sp.second});
            drawScoreSign(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f), {240, 250},
                          0.5f * powf(2.5f, sp2.first), "BEST",
                          String::from(gs->outerCtx->bestScore), {1.f, 0.55f, 0.02f, sp2.second});

            if (dead->showPrompt) {
                float yOffset = min(20.f, -dc->fullVF.bounds2D.mins.y / 2);
                TextBuffers playAgain = generateTextBuffers(a->sdfFont, "TAP TO PLAY AGAIN");
                drawText(a->sdfCommon, a->sdfFont, playAgain,
                         Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                             Float4x4::makeTranslation({244, 12 - yOffset, 0}) *
                             Float4x4::makeScale(0.9f) *
                             Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                         {0.85f, 1.75f}, {0, 0, 0, 0.4f});
                drawText(a->sdfCommon, a->sdfFont, playAgain,
                         Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                             Float4x4::makeTranslation({240, 16 - yOffset, 0}) *
                             Float4x4::makeScale(0.9f) *
                             Float4x4::makeTranslation({-playAgain.xMid(), 0, 0}),
                         {0.75f, 16.f}, {1.f, 1.f, 1.f, 1.f});
            }

            {
                // Draw back button
                Float2 buttonPos = vf.bounds2D.topLeft() + Float2{44, -44};
                float scale = dead->backButton.getScale();
                a->shapeShader->draw(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                                         Float4x4::makeTranslation({buttonPos, 0}) *
                                         Float4x4::makeScale(36.f * scale),
                                     a->circleTexture.id, {0.f, 0.f, 0.f, 0.3f}, 16, a->quad);
                a->shapeShader->draw(Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                                         Float4x4::makeTranslation({buttonPos, 0}) *
                                         Float4x4::makeScale(24.f * scale),
                                     a->arrowTexture.id, {1, 1, 1, 1}, 16, a->quad);
            }
        }

        if (!showGameOver && !gs->mode.title()) {
            // Draw score
            float scoreTime = mix(gs->scoreTime[0], gs->scoreTime[1], dc->intervalFrac);
            float zoom = powf(1.5f, scoreTime * scoreTime);
            TextBuffers tb = generateTextBuffers(a->sdfFont, String::from(gs->score));
            Float4x4 zoomMat = Float4x4::makeTranslation({0, 10.f, 0}) *
                               Float4x4::makeScale({zoom, zoom, 1.f}) *
                               Float4x4::makeTranslation({0, -10.f, 0});
            drawText(a->sdfCommon, a->sdfFont, tb,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({244, 570, 0}) * Float4x4::makeScale(1.5f) *
                         zoomMat * Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                     {0.85f, 1.75f}, {0, 0, 0, 0.4f});
            drawText(a->sdfCommon, a->sdfFont, tb,
                     Float4x4::makeOrtho(vf.bounds2D, -1.f, 1.f) *
                         Float4x4::makeTranslation({240, 574, 0}) * Float4x4::makeScale(1.5f) *
                         zoomMat * Float4x4::makeTranslation({-tb.xMid(), 0, 0}),
                     {0.75f, 32.f}, {1.f, 1.f, 1.f, 1.f});
        }

        if (auto trans = gs->camera.transition()) {
            float opacity = applySimpleCubic(clamp(1.f - trans->param * 2.f, 0.f, 1.f));
            float premul = powf(1.f - trans->param, 2.5f);
            applyTitleScreen(dc, opacity, premul);
        }
    } else {
        applyTitleScreen(dc, 1.f, 0.f);
    }
}

void ensureRenderTargetSize(Texture* tex, RenderToTexture* rtt, const Float2& dims) {
    if (!tex->id || tex->dims() != dims) {
        // Create temporary buffer
        rtt->destroy();
        tex->destroy();
        SamplerParams params;
        params.minFilter = false;
        params.magFilter = false;
        params.repeatX = false;
        params.repeatY = false;
        params.sRGB = false;
        PLY_ASSERT(isRounded(dims));
        tex->init((u32) dims.x, (u32) dims.y, image::Format::RGBA, 1, params);
        rtt->init(*tex, true);
    }
}

void drawTitleScreenToTemp(TitleScreen* ts) {
    const Assets* a = Assets::instance;
    const DrawContext* dc = DrawContext::instance();
    float aspect = dc->fullVF.bounds2D.height() / dc->fullVF.bounds2D.width();

    Float2 vpSize = dc->fullVF.viewport.size();
    ensureRenderTargetSize(&ts->tempTex, &ts->tempRTT, vpSize);

    float ez = 1.f;
    Float4x4 extraZoom = Float4x4::identity();
    bool enablePrompt = true;
    if (auto trans = dc->gs->camera.transition()) {
        float t = trans->param + dc->fracTime;
        t = clamp(interpolateCubic(0.f, 0.f, 0.5f, 1.f, trans->param) * 2.5f, 0.f, 1.f);
        ez = powf(15.f, t);
        Float3 c = {0, -0.2f, 0};
        extraZoom = Float4x4::makeTranslation(c) * Float4x4::makeScale({ez, ez, 1.f}) *
                    Float4x4::makeTranslation(-c);
        enablePrompt = false;
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
    drawTitle(ts, extraZoom);
    {
        // Draw background
        float hypnoAngle = mix(ts->hypnoAngle[0], ts->hypnoAngle[1], dc->intervalFrac);
        float hypnoScale = powf(1.3f, mix(ts->hypnoZoom[0], ts->hypnoZoom[1], dc->intervalFrac));
        a->hypnoShader->draw(
            Float4x4::makeOrtho({{-3.f, -3.f * aspect}, {3.f, 3.f * aspect}}, -1.f, 0.01f) *
                Float4x4::makeTranslation({0, -1.2f, 0}) * Float4x4::makeScale(ez * hypnoScale),
            a->waveTexture.id, a->hypnoPaletteTexture, ez * hypnoScale, hypnoAngle);
    }
    for (const DrawMesh* dm : a->rays) {
        // Draw rays
        GL_CHECK(DepthRange(0.5, 0.5));
        float angle = mix(ts->raysAngle[0], ts->raysAngle[1], dc->intervalFrac);
        Rect rayFrustum = dc->fullVF.frustum * (0.751708f / dc->fullVF.frustum.maxs.x);
        a->rayShader->draw(extraZoom * Float4x4::makeProjection(rayFrustum, 0.01f, 2.f) *
                               Float4x4::makeRotation({1, 0, 0}, -0.28f * Pi) *
                               Float4x4::makeTranslation({0, 0.55f, -1}) *
                               Float4x4::makeScale(2.f) * Float4x4::makeRotation({0, 0, 1}, angle),
                           dm);
    }
    drawStars(ts, extraZoom);
    // Draw prompt
    float footerY = dc->fullVF.bounds2D.mins.y;
    float promptY =
        mix(10.f, -150.f, clamp(unmix(-10.f, -240.f, dc->fullVF.bounds2D.mins.y), 0.f, 1.f));
    if (ts->showPrompt && enablePrompt) {
        TextBuffers tapToPlay = generateTextBuffers(a->sdfFont, "TAP TO PLAY");
        drawText(a->sdfCommon, a->sdfFont, tapToPlay,
                 extraZoom * Float4x4::makeOrtho(dc->fullVF.bounds2D, -1.f, 1.f) *
                     Float4x4::makeTranslation({308, 20 + promptY, 0}) * Float4x4::makeScale(1.1f) *
                     Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                 {0.85f, 1.75f}, {0.05f, 0.05f, 0.05f, 0.8f});
        drawOutlinedText(a->sdfOutline, a->sdfFont, tapToPlay,
                         extraZoom * Float4x4::makeOrtho(dc->fullVF.bounds2D, -1.f, 1.f) *
                             Float4x4::makeTranslation({304, 24 + promptY, 0}) *
                             Float4x4::makeScale(1.1f) *
                             Float4x4::makeTranslation({-tapToPlay.xMid(), 0, 0}),
                         {1, 1, 1, 0}, {0, 0, 0, 0}, {{0.6f, 16.f}, {0.75f, 12.f}});
    }

    // Draw open source button
    {
        float pulsate = 1.f;
        if (dc->gs->titleScreen->osb.button.state.up()) {
            float t = ts->osb.pulsateTime + dc->fracTime;
            if (t < 0.75f) {
            } else if (t < 1.f) {
                pulsate = mix(1.f, 0.85f, applySimpleCubic(unmix(0.75f, 1.f, t)));
            } else if (t < 1.15f) {
                pulsate = mix(0.85f, 1.25f, applySimpleCubic(unmix(1.f, 1.15f, t)));
            } else if (t < 1.4f) {
                pulsate = mix(1.25f, 1.f, applySimpleCubic(unmix(1.15f, 1.4f, t)));
            }
        } else {
            pulsate = dc->gs->titleScreen->osb.button.getScale();
        }
        Float4x4 b2w = extraZoom * Float4x4::makeOrtho(dc->fullVF.bounds2D, -1.f, 1.f) *
                       Float4x4::makeTranslation({62, 56 + footerY, 0}) *
                       Float4x4::makeRotation({0, 0, 1}, 0.1f) *
                       Float4x4::makeScale(pulsate * 0.8f);
        for (const DrawMesh* dm : a->stamp) {
            a->flatShader->draw(b2w * Float4x4::makeTranslation({0, 0, 0}) *
                                    Float4x4::makeScale({76, 66, 1}) *
                                    Float4x4::makeRotation({0, 0, 1}, ts->osb.angle),
                                dm, false, false);
        }
        TextBuffers thisGameIs = generateTextBuffers(a->sdfFont, "THIS GAME IS");
        drawText(a->sdfCommon, a->sdfFont, thisGameIs,
                 b2w * Float4x4::makeTranslation({0, 31, 0}) * Float4x4::makeScale(0.45f) *
                     Float4x4::makeTranslation({-thisGameIs.xMid(), 0, 0}),
                 {0.75f, 8.f}, {0.60f, 0.40f, 0.029f, 1.f});
        TextBuffers open = generateTextBuffers(a->sdfFont, "OPEN");
        drawText(a->sdfCommon, a->sdfFont, open,
                 b2w * Float4x4::makeTranslation({0, 6, 0}) * Float4x4::makeScale(0.7f) *
                     Float4x4::makeTranslation({-open.xMid(), 0, 0}),
                 {0.75f, 12.f}, {Float3{0.60f, 0.40f, 0.029f} * 0.5f, 1.f});
        TextBuffers source = generateTextBuffers(a->sdfFont, "SOURCE!");
        drawText(a->sdfCommon, a->sdfFont, source,
                 b2w * Float4x4::makeTranslation({0, -16, 0}) * Float4x4::makeScale(0.7f) *
                     Float4x4::makeTranslation({-source.xMid(), 0, 0}),
                 {0.75f, 12.f}, {Float3{0.60f, 0.40f, 0.029f} * 0.5f, 1.f});
        TextBuffers tapHere = generateTextBuffers(a->sdfFont, "TAP HERE");
        drawText(a->sdfCommon, a->sdfFont, tapHere,
                 b2w * Float4x4::makeTranslation({0, -34, 0}) * Float4x4::makeScale(0.45f) *
                     Float4x4::makeTranslation({-tapHere.xMid(), 0, 0}),
                 {0.75f, 8.f}, {0.60f, 0.40f, 0.029f, 1.f});
        TextBuffers forInfo = generateTextBuffers(a->sdfFont, "FOR INFO");
        drawText(a->sdfCommon, a->sdfFont, forInfo,
                 b2w * Float4x4::makeTranslation({0, -50, 0}) * Float4x4::makeScale(0.45f) *
                     Float4x4::makeTranslation({-forInfo.xMid(), 0, 0}),
                 {0.75f, 8.f}, {0.60f, 0.40f, 0.029f, 1.f});
    }

    {
        // Draw copyright
        TextBuffers copyright = generateTextBuffers(a->sdfFont, "@ 2020 Arc80 Software Inc.");
        drawText(a->sdfCommon, a->sdfFont, copyright,
                 extraZoom * Float4x4::makeOrtho(dc->fullVF.bounds2D, -1.f, 1.f) *
                     Float4x4::makeTranslation({306, 4 + promptY, 0}) * Float4x4::makeScale(0.36f) *
                     Float4x4::makeTranslation({-copyright.xMid(), 0, 0}),
                 {0.75f, 10.f}, {0.05f, 0.05f, 0.05f, 1});
    }

    GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, prevFBO));
}

ViewportFrustum getViewportFrustum(const Float2& fbSize) {
    float minAspect = 4.f / 3.f;
    float aspect = clamp(fbSize.y / fbSize.x, minAspect, 7.f / 3.f);
    Rect frustumRect = expand(Rect{{0, 0}}, Float2{1.f, aspect} * 0.148594f);
    Rect bounds2D = (Rect{{0, 0}, {1.f, aspect}} - Float2{0, (aspect - minAspect) / 2}) * 480.f;
    return fitFrustumInViewport({{0, 0}, fbSize}, frustumRect, bounds2D).snappedToPixels();
}

void render(GameFlow* gf, const Float2& fbSize, float renderDT, bool useManualColorCorrection) {
    PLY_ASSERT(fbSize.x > 0 && fbSize.y > 0);
    const Assets* a = Assets::instance;
    PLY_SET_IN_SCOPE(DynamicArrayBuffers::instance, &gf->dynBuffers);
    gf->dynBuffers.beginFrame();
    float intervalFrac = gf->fracTime / gf->simulationTimeStep;

#if !PLY_TARGET_IOS && !PLY_TARGET_ANDROID // doesn't exist in OpenGLES 3
    GL_CHECK(Enable(GL_FRAMEBUFFER_SRGB));
#endif

    // Enable face culling
    GL_CHECK(Enable(GL_CULL_FACE));
    GL_CHECK(CullFace(GL_BACK));
    GL_CHECK(FrontFace(GL_CCW));

    // Fit frustum in viewport
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    ViewportFrustum fullVF = getViewportFrustum(fbSize);

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

    if (useManualColorCorrection) {
        ensureRenderTargetSize(&gf->fullScreenTex, &gf->fullScreenRTT, fbSize);
        GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, gf->fullScreenRTT.fboID));
    }

    // Clear viewport
    GL_CHECK(Viewport(0, 0, (GLsizei) fbSize.x, (GLsizei) fbSize.y));
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
        dc.renderDT = renderDT;
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
        float divWidth = roundNearest(max(1.f, fullVF.viewport.width() / 60.f));
        float divRight = fullVF.viewport.mins.x + (fullVF.viewport.width() + divWidth) * slide;
        float divLeft = divRight - divWidth;

        // Draw left (new) panel
        {
            ViewportFrustum leftVF = fullVF;
            leftVF.viewport.mins.x = divLeft - fullVF.viewport.width();
            leftVF.viewport.maxs.x = divLeft;
            leftVF = leftVF.clip(fullVF.viewport).snappedToPixels();

            if (!leftVF.viewport.isEmpty()) {
                renderPanel(gf->gameState, leftVF);
            }
        }

        // Draw vertical bar
        {
            Rect barRect =
                roundNearest(intersect(fullVF.viewport, Rect{{divLeft, fullVF.viewport.mins.y},
                                                             {divRight, fullVF.viewport.maxs.y}}));
            if (!barRect.isEmpty()) {
                GL_CHECK(Viewport((GLint) barRect.mins.x, (GLint) barRect.mins.y,
                                  (GLsizei) barRect.width(), (GLsizei) barRect.height()));
                a->flatShader->drawQuad(Float4x4::identity(), {1, 1, 1, 1}, false);
            }
        }

        // Draw right (old) panel
        {
            ViewportFrustum rightVF = fullVF;
            rightVF.viewport.mins.x = divRight;
            rightVF.viewport.maxs.x = divRight + fullVF.viewport.width();
            rightVF = rightVF.clip(fullVF.viewport).snappedToPixels();
            if (!rightVF.viewport.isEmpty()) {
                PLY_ASSERT(!trans->oldGameState->mode.title());
                renderPanel(trans->oldGameState, rightVF);
            }
        }
    } else {
        renderPanel(gf->gameState, fullVF);
    }

    if (useManualColorCorrection) {
        // Copy to default framebuffer with color correction
        GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CHECK(Viewport(0, 0, (GLsizei) fbSize.x, (GLsizei) fbSize.y));
        GL_CHECK(DepthRange(0.0, 1.0));
        GL_CHECK(DepthMask(GL_TRUE));
        GL_CHECK(ClearColor(0, 0, 0, 1));
        GL_CHECK(ClearDepth(1.0));
        GL_CHECK(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        a->colorCorrectShader->draw(a->quad, gf->fullScreenTex.id);
    }
}

} // namespace flap
