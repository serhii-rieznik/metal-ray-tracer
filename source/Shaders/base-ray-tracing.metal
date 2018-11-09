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

kernel void generateRays(device Ray* rays [[buffer(0)]],
                         device vector_float4* noise [[buffer(1)]],
                         constant ApplicationData& appData [[buffer(2)]],
                         uint2 coordinates [[thread_position_in_grid]],
                         uint2 size [[threads_per_grid]])
{
#if (SCENE <= SCENE_CORNELL_BOX_MAX)
    float3 up = float3(0.0f, 1.0f, 0.0f);
    const float3 origin = float3(0.0f, 1.0f, 2.35f);
    const float3 target = float3(0.0f, 1.0f, 0.0f);
    const float fov = 90.0f * PI / 180.0f;
#elif (SCENE == SCENE_VEACH_MIS)
    float3 up = float3(0.0f, 0.952424f, -0.304776f);
    const float3 origin = float3(0.0f, 2.0f, 15.0f);
    const float3 target = float3(0.0f, 1.69522f, 14.0476f);
    const float fov = 36.7774f * PI / 180.0f;
#elif (SCENE == SCENE_SPHERE)
    float3 up = float3(0.0f, 1.0, 0.0);
    const float3 origin = float3(0.0f, 0.0f, 100.0f);
    const float3 target = float3(0.0f, 0.0f, 0.0);
    const float fov = 50.0f * PI / 180.0f;
#elif (SCENE == SCENE_PBS_SPHERES)
    float3 up = float3(0.0f, 1.0, 0.0);
    const float3 origin = float3(0.0f, 75.0f, -175.0f);
    const float3 target = float3(0.0f, 10.0f, 0.0f);
    const float fov = (47.5 + 0.125) * PI / 180.0f;
#else
#   error No scene is defined
#endif

    uint rayIndex = coordinates.x + coordinates.y * size.x;
    Ray r = rays[rayIndex];
    uint comp = r.completed;
    uint fi = appData.frameIndex;
    if ((fi == 0) || (comp != 0))
    {
        float3 direction = normalize(target - origin);
        float3 side = cross(direction, up);
        up = cross(side, direction);
        float fovScale = tan(fov / 2.0f);

        uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + NOISE_BLOCK_SIZE * (coordinates.y % NOISE_BLOCK_SIZE);
        device const float4& randomSample = noise[randomSampleIndex];

        float aspect = float(size.y) / float(size.x);
        float2 uv = float2(coordinates) / float2(size - 1) * 2.0f - 1.0f;
        float2 rnd = (randomSample.xy * 2.0 - 1.0) / float2(size);
        float ax = fovScale * (uv.x + rnd.x);
        float ay = fovScale * (uv.y + rnd.y) * aspect;
        float az = 1.0f;

        r.base.origin = origin;
        r.base.direction = normalize(ax * side + ay * up + az * direction);
        r.base.minDistance = DISTANCE_EPSILON;
        r.base.maxDistance = INFINITY;
        r.radiance = 0.0f;
        r.throughput = 1.0f;
        r.misPdf = 0.0f;
        r.bounces = 0;
        r.completed = 0;
        r.generation = (appData.frameIndex == 0) ? 0 : (r.generation + 1);
        rays[rayIndex] = r;
    }
}

kernel void handleIntersections(texture2d<float> environment [[texture(0)]],
                                device const Intersection* intersections [[buffer(0)]],
                                device const Material* materials [[buffer(1)]],
                                device const Triangle* triangles [[buffer(2)]],
                                device const EmitterTriangle* emitterTriangles [[buffer(3)]],
                                device const Vertex* vertices [[buffer(4)]],
                                device packed_uint3* indices [[buffer(5)]],
                                device const RandomSample* noise [[buffer(6)]],
                                device Ray* rays [[buffer(7)]],
                                constant ApplicationData& appData [[buffer(8)]],
                                uint2 coordinates [[thread_position_in_grid]],
                                uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device Ray& currentRay = rays[rayIndex];

    if (currentRay.completed)
        return;

    device const Intersection& i = intersections[rayIndex];

    if (i.distance < 0.0f)
    {
        currentRay.radiance += currentRay.throughput *
            (appData.environmentColor + sampleEnvironment(environment, currentRay.base.direction));

        currentRay.base.maxDistance = -1.0;
        currentRay.throughput = 0.0;
        currentRay.completed = 1;
        return;
    }

    uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE;

    device const RandomSample& randomSample = noise[randomSampleIndex];
    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    device const packed_uint3& triangleIndices = indices[i.primitiveIndex];
    device const Vertex& a = vertices[triangleIndices.x];
    device const Vertex& b = vertices[triangleIndices.y];
    device const Vertex& c = vertices[triangleIndices.z];
    
    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

    if ((currentRay.bounces < MAX_PATH_LENGTH) && (dot(material.emissive, material.emissive) > 0.0))
    {
#   if (IS_MODE == IS_MODE_MIS)
        float lightSamplePdf = lightSamplingPdf(currentVertex.n, currentVertex.v - currentRay.base.origin, triangle.area);
        float3 weight = currentRay.throughput *
            ((currentRay.bounces == 0) ? 1.0f : powerHeuristic(currentRay.misPdf, lightSamplePdf));
        weight *= float(dot(currentRay.base.direction, currentVertex.n) < 0.0f);
#   elif (IS_MODE == IS_MODE_BSDF)
        float3 weight = currentRay.throughput;
        weight *= float(dot(currentRay.base.direction, currentVertex.n) < 0.0f);
#   elif (IS_MODE == IS_MODE_LIGHT)
        float3 weight = float(currentRay.bounces == 0);
#   endif

        currentRay.radiance += material.emissive * weight;
    }

    // generate next bounce
    {
        SampledMaterial materialSample = sampleMaterial(material, currentVertex.n, currentRay.base.direction, randomSample);
        currentRay.base.origin = currentVertex.v + materialSample.direction * DISTANCE_EPSILON;
        currentRay.base.direction = materialSample.direction;
        currentRay.throughput *= materialSample.weight;
        currentRay.misPdf = materialSample.pdf;
        currentRay.bounces += 1;
        currentRay.completed = (currentRay.bounces >= MAX_PATH_LENGTH);
    }

#if (ENABLE_RUSSIAN_ROULETTE)
    if (currentRay.bounces >= 5)
    {
        float m = max(currentRay.throughput.x, max(currentRay.throughput.y, currentRay.throughput.z));
        float q = min(0.95f, m);
        if (randomSample.rrSample >= q)
        {
            currentRay.completed = 1;
        }
        else
        {
            currentRay.throughput *= 1.0f / q;
        }
    }
#endif
}
