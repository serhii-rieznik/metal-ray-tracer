//
//  compute-shader-test.metal
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
    image.write(float4(), coordinates);
}
