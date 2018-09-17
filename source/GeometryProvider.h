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

    id<MTLBuffer> getIndexBuffer() const { return _indexBuffer; }
    id<MTLBuffer> getVertexBuffer() const { return _vertexBuffer; }

private:
    id<MTLBuffer> _indexBuffer = nil;
    id<MTLBuffer> _vertexBuffer = nil;
};
