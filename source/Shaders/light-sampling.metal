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

    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

    uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE;
    device const RandomSample& randomSample = noise[randomSampleIndex];

    device const EmitterTriangle& emitterTriangle =
        sampleEmitterTriangle(emitterTriangles, appData.emitterTrianglesCount, randomSample.emitterSample);

    float3 lightTriangleBarycentric = barycentric(randomSample.barycentricSample);
    Vertex lightVertex = interpolate(emitterTriangle.v0, emitterTriangle.v1, emitterTriangle.v2, lightTriangleBarycentric);

    packed_float3 directionToLight = lightVertex.v - currentVertex.v;
    float distanceToLightSquared = dot(directionToLight, directionToLight);
    directionToLight = normalize(directionToLight);

    float DdotN = dot(directionToLight, currentVertex.n);
    float DdotLN = -dot(directionToLight, lightVertex.n);
    float lightSamplePdf = emitterTriangle.pdf * distanceToLightSquared / (emitterTriangle.area * DdotLN);

    SampledMaterial materialSample = evaluateMaterial(material, currentVertex.n, currentRay.base.direction,
        directionToLight, randomSample);

#if (IS_MODE == IS_MODE_MIS)
    float weight = powerHeuristic(lightSamplePdf, materialSample.pdf);
#elif (IS_MODE == IS_MODE_LIGHT)
    float weight = 1.0f;
#elif (IS_MODE == IS_MODE_BSDF)
    float weight = 0.0f;
#endif

    bool triangleSampled = (DdotN > 0.0f) && (DdotLN > 0.0f) && (dot(materialSample.bsdf, materialSample.bsdf) > 0.0f);

    device LightSamplingRay& lightSamplingRay = lightSamplingRays[rayIndex];
    lightSamplingRay.base.origin = currentVertex.v + directionToLight * DISTANCE_EPSILON;
    lightSamplingRay.base.direction = directionToLight;
    lightSamplingRay.base.minDistance = DISTANCE_EPSILON;
    lightSamplingRay.base.maxDistance = triangleSampled ? INFINITY : -1.0;
    lightSamplingRay.targetPrimitiveIndex = emitterTriangle.globalIndex;
    lightSamplingRay.throughput = currentRay.throughput * (emitterTriangle.emissive / lightSamplePdf) * materialSample.bsdf * weight;
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
        rays[rayIndex].radiance += lightSamplingRays[rayIndex].throughput;
    }
}
