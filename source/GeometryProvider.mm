//
//  GeometryProvider.mm
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/17/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include "GeometryProvider.h"
#include "Shaders/structures.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"

void GeometryProvider::loadFile(const std::string&, id<MTLDevice> device)
{
    Vertex vertices[3] = {
        { { 0.25f, 0.25f, 0.0f }, {}, {} },
        { { 0.75f, 0.25f, 0.0f }, {}, {} },
        { { 0.50f, 0.75f, 0.0f }, {}, {} }
    };
    _vertexBuffer = [device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeManaged];

    uint32_t indices[3] = { 0, 1, 2 };
    _indexBuffer = [device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeManaged];
}

