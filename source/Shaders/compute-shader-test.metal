//
//  Shaders.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>

using namespace metal;

kernel void imageFillTest(texture2d<float, access::write> image [[texture(0)]],
                     uint2 coordinates [[thread_position_in_grid]],
                     uint2 size [[threads_per_grid]])
{
    float2 uv = float2(coordinates) / float2(size - 1);
    image.write(float4(uv, 0.0, 1.0), coordinates);
}
