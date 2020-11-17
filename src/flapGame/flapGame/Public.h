#pragma once
#if !PLY_TARGET_IOS
#include <flapGame/Config.h>
#endif
#include <ply-math/Base.h>
#include <ply-runtime/Base.h>

namespace flap {

using namespace ply;

struct GameFlow;

void init(StringView assetsPath);
void reloadAssets();
void shutdown();
GameFlow* createGameFlow();
void destroy(GameFlow* gf);
void update(GameFlow* gf, float dt);
void doInput(GameFlow* gf, const Float2& fbSize, const Float2& pos, bool down,
             float swipeMargin = 0.f);
void togglePause(GameFlow* gf);
void onBackPressed(GameFlow* gf);
void stopMusic(GameFlow* gf);
void render(GameFlow* gf, const Float2& fbSize, float renderDT,
            bool useManualColorCorrection = false);
void onAppDeactivate(GameFlow* gf);
void onAppActivate(GameFlow* gf);

} // namespace flap
