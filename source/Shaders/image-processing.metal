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

kernel void accumulateImage(texture2d<float, access::read_write> image [[texture(0)]],
                            device Ray* rays [[buffer(0)]],
                            constant ApplicationData& appData [[buffer(1)]],
                            uint2 coordinates [[thread_position_in_grid]],
                            uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    Ray currentRay = rays[rayIndex];
    if (currentRay.completed)
    {
        float4 outputColor = float4(rays[rayIndex].radiance, 1.0);

        if (any(isnan(outputColor)))
            outputColor = float4(1000.0, 0.0, 1000.0, 1.0);

        if (any(isinf(outputColor)))
            outputColor = float4(0.0, 1000.0, 1000.0, 1.0);

        //*
        if (any(outputColor < 0.0f))
            outputColor = float4(0.0, 1000.0, 1000.0, 1.0);
        // */

        /*
         float lumOut = dot(outputColor, float4(LUMINANCE_VECTOR, 0.0f));
         outputColor = lumOut > 0.5f ? float4(0.0f, lumOut - 0.5f, 0.0f, 1.0f) :  float4(0.5 - lumOut, 0.0f, 0.0f, 1.0f);
         // */

#if (ENABLE_IMAGE_ACCUMULATION)
        uint index = currentRay.generation;
        if (index > 0)
        {
            float t = 1.0f / float(index + 1.0f);
            float4 storedColor = image.read(coordinates);
            outputColor = mix(storedColor, outputColor, t);
        }
#endif

        image.write(outputColor, coordinates);
    }
}

