#pragma once

#include "Shaders/structures.h"
#include <vector>
#include <string>

namespace TextureProvider {

id<MTLTexture> loadFile(const char*, id<MTLDevice>);

}
