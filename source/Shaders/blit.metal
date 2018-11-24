//
//  blit.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>
#include "structures.h"

using namespace metal;

struct BlitVertexOut
{
    float4 pos [[position]];
    float2 coords;
};

constant constexpr static const float4 fullscreenTrianglePositions[3]
{
    {-1.0, -1.0, 0.0, 1.0},
    { 3.0, -1.0, 0.0, 1.0},
    {-1.0,  3.0, 0.0, 1.0}
};

vertex BlitVertexOut blitVertex(uint vertexIndex [[vertex_id]])
{
    BlitVertexOut out;
    out.pos = fullscreenTrianglePositions[vertexIndex];
    out.coords = out.pos.xy * 0.5 + 0.5;
    return out;
}

fragment float4 blitFragment(BlitVertexOut in [[stage_in]],
                             constant ApplicationData& appData [[buffer(0)]],
                             texture2d<float> image [[texture(0)]],
                             texture2d<float> reference [[texture(1)]]
                             )
{
    constexpr sampler defaultSampler(coord::normalized, filter::nearest);
    float4 color = image.sample(defaultSampler, in.coords);

    if (appData.comparisonMode != COMPARE_DISABLED)
    {
        float4 referenceImageColor = reference.sample(defaultSampler, float2(in.coords.x, 1.0f - in.coords.y));
        float4 neutralColor = 0.5f;

        float4 referenceColor = (appData.comparisonMode == COMPARE_TO_REFERENCE) ? referenceImageColor : neutralColor;

        float colorLum = dot(color, float4(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f));
        float referenceLum = dot(referenceColor, float4(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f));
        float diff = (colorLum - referenceLum);
        color = (diff > 0.0f) ? float4(0.0f, diff, 0.0f, 1.0f) : float4(-diff, 0.0f, 0.0f, 1.0f);
    }

    return color;
}
