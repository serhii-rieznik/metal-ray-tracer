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
                         device const RandomSample* noise [[buffer(1)]],
                         constant ApplicationData& appData [[buffer(2)]],
                         uint2 coordinates [[thread_position_in_grid]],
                         uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device Ray& r = rays[rayIndex];
    if ((appData.frameIndex == 0) || (r.completed != 0))
    {
        float3 up = float3(0.0f, 1.0f, 0.0f);
        float3 direction = normalize(appData.camera.target - appData.camera.origin);
        float3 side = cross(direction, up);
        up = cross(side, direction);
        float fovScale = tan(appData.camera.fov / 2.0f);

        uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE;
        device const RandomSample& randomSample = noise[randomSampleIndex];

        float aspect = float(size.y) / float(size.x);
        float2 uv = float2(coordinates) / float2(size - 1) * 2.0f - 1.0f;

        float2 r1 = randomSample.pixelSample;
        float2 rnd = (r1 * 2.0f - 1.0f) / float2(size);
        float ax = fovScale * (uv.x) + rnd.x;
        float ay = fovScale * (uv.y) * aspect + rnd.y;
        float az = 1.0f;

        r.base.origin = appData.camera.origin;
        r.base.direction = normalize(ax * side + ay * up + az * direction);
        r.base.minDistance = DISTANCE_EPSILON;
        r.base.maxDistance = INFINITY;
        r.radiance = 0.0f;
        r.bounces = 0;
        r.throughput = 1.0f;
        r.misPdf = 1.0f;
        r.eta = 1.0f;
        r.generation = (appData.frameIndex == 0) ? 0 : (r.generation + 1);
        r.completed = 0;
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
        currentRay.radiance += currentRay.throughput * (appData.environmentColor + sampleEnvironment(environment, currentRay.base.direction));
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

    if ((currentRay.completed == 0) && (dot(material.emissive, material.emissive) > 0.0))
    {
        float3 direction = currentVertex.v - currentRay.base.origin;
        float samplePdf = triangle.discretePdf * directSamplingPdf(currentVertex.n, direction, triangle.area);

        float weights[3] = {
            MIS_ENABLE_BSDF_SAMPLING ? powerHeuristic(currentRay.misPdf, samplePdf) : 0.0f, // IS_MODE_MIS
            0.0f, // IS_MODE_LIGHT
            1.0f, // IS_MODE_BSDF
        };

        float weight = float(currentRay.bounces == 0) ? 1.0f : weights[IS_MODE];
        weight *= float(dot(currentRay.base.direction, currentVertex.n) < 0.0f);
        currentRay.radiance += material.emissive * currentRay.throughput * weight;
    }

    // generate next bounce
    {
        SampledMaterial materialSample = sampleMaterial(material, currentVertex.n, currentRay.base.direction, randomSample);

        if (materialSample.valid)
        {
            currentRay.base.origin = currentVertex.v + materialSample.direction * DISTANCE_EPSILON;
            currentRay.base.direction = materialSample.direction;
            currentRay.throughput *= materialSample.weight;
            currentRay.misPdf = materialSample.pdf;
            currentRay.bounces += 1;
            currentRay.eta *= materialSample.eta;
        }
        else
        {
            currentRay.base.maxDistance = -1.0f;
            currentRay.throughput = 0.0f;
            currentRay.misPdf = 0.0f;
        }
        currentRay.completed = uint(materialSample.valid == 0) + uint(currentRay.bounces >= MAX_PATH_LENGTH);
    }

#if (ENABLE_RUSSIAN_ROULETTE)
    if (currentRay.bounces >= 5)
    {
        float m = max(currentRay.throughput.x, max(currentRay.throughput.y, currentRay.throughput.z));
        float q = min(m * sqr(currentRay.eta), 0.95f);
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
