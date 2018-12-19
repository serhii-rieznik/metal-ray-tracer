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
                            device const Ray* rays [[buffer(0)]],
                            constant ApplicationData& appData [[buffer(1)]],
                            device const packed_float3* xyz [[buffer(2)]],
                            uint2 coordinates [[thread_position_in_grid]],
                            uint2 size [[threads_per_grid]])
{
    if (appData.frameIndex == 0)
        image.write(0.0f, coordinates);

    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Ray& currentRay = rays[rayIndex];
    if (currentRay.completed && (currentRay.generation < MAX_SAMPLES))
    {
        float4 outputColorXYZ = GPUSpectrumToXYZ(currentRay.radiance, xyz);
        float4 outputColor = XYZtoRGB(outputColorXYZ);

        if (any(isnan(outputColor)))
            outputColor = float4(1000.0f, 0.0f, 1000.0, 1.0f);

        if (any(isinf(outputColor)))
            outputColor = float4(1000.0f, 0.0f, 0.0f, 1.0f);

        //*
        if (any(outputColor < 0.0f))
            outputColor = float4(0.0f, 1000.0f, 1000.0, 1.0f);
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

