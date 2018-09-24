//
//  base-ray-tracing.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>
#include "structures.h"

using namespace metal;

#define ENABLE_ACCUMULATION 1

kernel void accumulateImage(texture2d<float, access::read_write> image [[texture(0)]],
                            device Ray* rays [[buffer(0)]],
                            constant ApplicationData& appData [[buffer(1)]],
                            uint2 coordinates [[thread_position_in_grid]],
                            uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    float4 outputColor = float4(rays[rayIndex].radiance, 1.0);
#if (ENABLE_ACCUMULATION)
    if (appData.frameIndex > 0)
    {
        float4 storedColor = image.read(coordinates);
        outputColor = mix(outputColor, storedColor, float(appData.frameIndex) / float(appData.frameIndex + 1));
    }
#endif
    image.write(outputColor, coordinates);
}

