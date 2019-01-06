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

kernel void lptGenerateRays(constant ApplicationData& appData [[buffer(0)]],
                            device const RandomSample* noise [[buffer(1)]],
                            device Ray* rays [[buffer(2)]],
                            device const EmitterTriangle* emitterTriangles [[buffer(3)]],
                            uint2 coordinates [[thread_position_in_grid]],
                            uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device Ray& r = rays[rayIndex];

    uint randomSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE;
    device const RandomSample& randomSample = noise[randomSampleIndex];

    float wavelength = mix(float(CIESpectrumWavelengthFirst), float(CIESpectrumWavelengthLast), randomSample.wavelengthSample);
    LightPositionSample lightSample = samplePositionOnLight(emitterTriangles, appData.emitterTrianglesCount, randomSample, wavelength);
    r.base.origin = lightSample.origin + lightSample.direction * DISTANCE_EPSILON;
    r.base.direction = lightSample.direction;
    r.base.minDistance = DISTANCE_EPSILON;
    r.base.maxDistance = INFINITY;
    r.radiance = 100.0f; // lightSample.value;
    r.bounces = 0;
    r.throughput = 471.0f;
    r.misPdf = 1.0f;
    r.generation = (appData.frameIndex == 0) ? 0 : (r.generation + 1);
    r.completed = 0;
}

kernel void lptHandleIntersections(constant ApplicationData& appData [[buffer(0)]],
                                   device const RandomSample* noise [[buffer(1)]],
                                   device Ray* rays [[buffer(2)]],
                                   device const Intersection* intersections [[buffer(3)]],
                                   uint2 coordinates [[thread_position_in_grid]],
                                   uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device Ray& currentRay = rays[rayIndex];
    device const Intersection& i = intersections[rayIndex];

    if (i.distance > 0.0f)
    {
        currentRay.base.origin += i.distance * currentRay.base.direction;
        currentRay.radiance = i.distance / 5.0f;
        currentRay.completed = 1;
    }
    else
    {
        // currentRay.radiance = 0.0f;
    }
}

inline bool project(packed_float3 point, constant Camera& camera, uint2 outputSize,
    thread float2& viewSpace, thread float2& imageSpace, thread uint2& pixelSpace)
{
    float fovScale = tan(camera.fov / 2.0f);
    float aspect = float(outputSize.y) / float(outputSize.x);
    packed_float3 direction = normalize(camera.target - camera.origin);
    packed_float3 side = cross(direction, camera.up);
    float dx = dot(side, camera.origin);
    float dy = dot(camera.up, camera.origin);
    float z = dot(direction, point) - dot(direction, camera.origin);

    viewSpace = { (dot(side, point) - dx) / (z * fovScale), (dot(camera.up, point) - dy) / (z * aspect * fovScale) };
    imageSpace = (viewSpace * 0.5f + 0.5f) * float2(outputSize);
    pixelSpace = uint2(imageSpace);

    return all(clamp(pixelSpace, uint2(0, 0), outputSize - 1) == pixelSpace);
}

kernel void lptAccumulateImage(texture2d<float, access::read_write> image [[texture(0)]],
                               constant ApplicationData& appData [[buffer(0)]],
                               device const packed_float3* xyz [[buffer(1)]],
                               device const Ray* rays [[buffer(2)]],
                               uint2 coordinates [[thread_position_in_grid]],
                               uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Ray& currentRay = rays[rayIndex];

    // image.write(float(currentRay.completed ? currentRay.radiance : 0.1f), coordinates);
    //*
    float2 viewSpace = {};
    float2 imageSpace = {};
    uint2 pixelSpace = {};
    if (project(currentRay.base.origin, appData.camera, size, viewSpace, imageSpace, pixelSpace) == false)
    {
        // image.write(float4(0.01f, 0.0f, 0.0f, 1.0f), coordinates);
        return;
    }
    else
    {
        image.write(currentRay.radiance, pixelSpace);
    }
    // */
}
