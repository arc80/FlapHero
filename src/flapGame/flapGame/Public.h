#pragma once
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
void doInput(GameFlow* gf);
void render(GameFlow* gf, const IntVec2& fbSize);

} // namespace flap
