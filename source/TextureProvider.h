//
//  GeometryProvider.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/17/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "Shaders/structures.h"
#include <vector>
#include <string>

namespace TextureProvider {

id<MTLTexture> loadFile(const char*, id<MTLDevice>);

}
