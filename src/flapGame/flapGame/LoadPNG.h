#pragma once
#include <flapGame/Core.h>

namespace flap {

image::OwnImage loadPNG(ConstBufferView src, bool premultiply = true);

} // namespace flap
