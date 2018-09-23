//
//  GeometryProvider.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/17/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <Metal/Metal.h>
#include <string>

class GeometryProvider
{
public:
    void loadFile(const std::string&, id<MTLDevice>);

    id<MTLBuffer> indexBuffer() const { return _indexBuffer; }
    id<MTLBuffer> vertexBuffer() const { return _vertexBuffer; }
    id<MTLBuffer> materialBuffer() const { return _materialBuffer; }
    id<MTLBuffer> triangleBuffer() const { return _triangleBuffer; }

    uint32_t triangleCount() const { return _triangleCount; }

private:
    id<MTLBuffer> _indexBuffer = nil;
    id<MTLBuffer> _vertexBuffer = nil;
    id<MTLBuffer> _materialBuffer = nil;
    id<MTLBuffer> _triangleBuffer = nil;
    uint32_t _triangleCount = 0;
};
