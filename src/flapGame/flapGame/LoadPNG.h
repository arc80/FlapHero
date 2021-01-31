#pragma once
#include <flapGame/Core.h>

namespace flap {

image::OwnImage loadPNG(StringView src, bool premultiply = true);

} // namespace flap
