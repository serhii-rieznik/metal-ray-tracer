//
//  base-ray-tracing.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>
#include "structures.h"
#include "raytracing.h"
#include "materials.h"

using namespace metal;

kernel void generateLightSamplingRays(device const Intersection* intersections [[buffer(0)]],
                                      device const Material* materials [[buffer(1)]],
                                      device const Triangle* triangles [[buffer(2)]],
                                      device const EmitterTriangle* emitterTriangles [[buffer(3)]],
                                      device const Vertex* vertices [[buffer(4)]],
                                      device packed_uint3* indices [[buffer(5)]],
                                      device const RandomSample* noise [[buffer(6)]],
                                      device Ray* rays [[buffer(7)]],
                                      device LightSamplingRay* lightSamplingRays [[buffer(8)]],
                                      constant ApplicationData& appData [[buffer(9)]],
                                      uint2 coordinates [[thread_position_in_grid]],
                                      uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Ray& currentRay = rays[rayIndex];
    device const Intersection& i = intersections[rayIndex];
    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    device const packed_uint3& triangleIndices = indices[i.primitiveIndex];
    device const Vertex& a = vertices[triangleIndices.x];
    device const Vertex& b = vertices[triangleIndices.y];
    device const Vertex& c = vertices[triangleIndices.z];

    if (i.distance < 0)
    {
        device LightSamplingRay& lightSamplingRay = lightSamplingRays[rayIndex];
        lightSamplingRay.base.maxDistance = -1.0f;
        lightSamplingRay.targetPrimitiveIndex = uint(-2);
        return;
    }

    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

    uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE;
    device const RandomSample& randomSample = noise[randomSampleIndex];

    LightSample lightSample = sampleLight(currentVertex.v, currentVertex.n, emitterTriangles,
        appData.emitterTrianglesCount, randomSample);

    SampledMaterial materialSample = evaluateMaterial(material, currentVertex.n, currentRay.base.direction,
        lightSample.direction, randomSample);

#if (IS_MODE == IS_MODE_MIS)
    float weight = powerHeuristic(lightSample.pdf, materialSample.pdf);
#elif (IS_MODE == IS_MODE_LIGHT)
    float weight = 1.0f;
#elif (IS_MODE == IS_MODE_BSDF)
    float weight = 0.0f;
#endif

    device LightSamplingRay& lightSamplingRay = lightSamplingRays[rayIndex];
    lightSamplingRay.base.origin = currentVertex.v + lightSample.direction * DISTANCE_EPSILON;
    lightSamplingRay.base.direction = lightSample.direction;
    lightSamplingRay.base.minDistance = DISTANCE_EPSILON;
    lightSamplingRay.base.maxDistance = (materialSample.valid && lightSample.valid) ? INFINITY : -1.0;
    lightSamplingRay.targetPrimitiveIndex = (materialSample.valid && lightSample.valid) ? lightSample.primitiveIndex : uint(-2);
    lightSamplingRay.throughput = currentRay.throughput * lightSample.value * materialSample.bsdf * weight;
}

kernel void lightSamplingHandler(device const LightSamplingIntersection* intersections [[buffer(0)]],
                                 device const LightSamplingRay* lightSamplingRays [[buffer(1)]],
                                 device Ray* rays [[buffer(2)]],
                                 uint2 coordinates [[thread_position_in_grid]],
                                 uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    if (intersections[rayIndex].primitiveIndex == lightSamplingRays[rayIndex].targetPrimitiveIndex)
    {
        if (rays[rayIndex].bounces + 1 < MAX_PATH_LENGTH)
        {
            rays[rayIndex].radiance += lightSamplingRays[rayIndex].throughput;
        }
    }
}
