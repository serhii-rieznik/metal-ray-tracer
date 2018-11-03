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

#define ADD_RADIANCE 1

kernel void generateRays(device Ray* rays [[buffer(0)]],
                         device vector_float4* noise [[buffer(1)]],
                         constant ApplicationData& appData [[buffer(2)]],
                         uint2 coordinates [[thread_position_in_grid]],
                         uint2 size [[threads_per_grid]])
{
#if ((SCENE == SCENE_CORNELL_BOX) || (SCENE == SCENE_CORNELL_BOX_SPHERES))
    float3 up = float3(0.0f, 1.0f, 0.0f);
    const float3 origin = float3(0.0f, 1.0f, 2.35f);
    const float3 target = float3(0.0f, 1.0f, 0.0f);
    const float fov = PI / 2.0f;
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
    const float3 origin = float3(0.0f, 75.0f, 150.0f);
    const float3 target = float3(0.0f, 0.0f, 0.0);
    const float fov = 60.0 * PI / 180.0f;
#else
#   error No scene is defined
#endif

    float3 direction = normalize(target - origin);
    float3 side = cross(direction, up);
    up = cross(side, direction);
    float fovScale = tan(fov / 2.0f);

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + NOISE_BLOCK_SIZE * (coordinates.y % NOISE_BLOCK_SIZE);
    device const float4& noiseSample = noise[noiseSampleIndex];

    float aspect = float(size.y) / float(size.x);
    float2 uv = float2(coordinates) / float2(size - 1) * 2.0f - 1.0f;
    float2 rnd = (noiseSample.xy * 2.0 - 1.0) / float2(size - 1);
    float ax = fovScale * (uv.x + rnd.x);
    float ay = fovScale * (uv.y + rnd.y) * aspect;
    float az = 1.0f;

    uint rayIndex = coordinates.x + coordinates.y * size.x;
    rays[rayIndex].base.origin = origin;
    rays[rayIndex].base.direction = normalize(ax * side + ay * up + az * direction);
    rays[rayIndex].base.minDistance = DISTANCE_EPSILON;
    rays[rayIndex].base.maxDistance = INFINITY;
    rays[rayIndex].radiance = 0.0f;
    rays[rayIndex].throughput = 1.0f;
    rays[rayIndex].misPdf = 1.0f;
    rays[rayIndex].bounces = 0;
}

kernel void handleIntersections(texture2d<float> environment [[texture(0)]],
                                device const Intersection* intersections [[buffer(0)]],
                                device const Material* materials [[buffer(1)]],
                                device const Triangle* triangles [[buffer(2)]],
                                device const EmitterTriangle* emitterTriangles [[buffer(3)]],
                                device const Vertex* vertices [[buffer(4)]],
                                device packed_uint3* indices [[buffer(5)]],
                                device const float4* noise [[buffer(6)]],
                                device Ray* rays [[buffer(7)]],
                                device LightSamplingRay* lightSamplingRays [[buffer(8)]],
                                constant ApplicationData& appData [[buffer(9)]],
                                uint2 coordinates [[thread_position_in_grid]],
                                uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Intersection& i = intersections[rayIndex];
    device Ray& currentRay = rays[rayIndex];
    device LightSamplingRay& lightSamplingRay = lightSamplingRays[rayIndex];

    if (i.distance < DISTANCE_EPSILON)
    {
    #if (ADD_RADIANCE)
        currentRay.radiance += currentRay.throughput *
            (appData.environmentColor + sampleEnvironment(environment, currentRay.base.direction));
    #endif

        currentRay.base.maxDistance = -1.0;
        currentRay.throughput = 0.0;
        lightSamplingRay.base.maxDistance = -1.0;
        lightSamplingRay.targetPrimitiveIndex = uint(-1);
        return;
    }

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE +
        currentRay.bounces * NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE;

    device const float4& noiseSample = noise[noiseSampleIndex];
    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    device const packed_uint3& triangleIndices = indices[i.primitiveIndex];
    device const Vertex& a = vertices[triangleIndices.x];
    device const Vertex& b = vertices[triangleIndices.y];
    device const Vertex& c = vertices[triangleIndices.z];
    
    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

#if (ADD_RADIANCE)
    if (dot(material.emissive, material.emissive) > 0.0)
    {
    #if (IS_MODE == IS_MODE_MIS)
        packed_float3 directionToLight = currentVertex.v - currentRay.base.origin;
        float distanceToLight = length(directionToLight);
        directionToLight /= distanceToLight;
        float cosTheta = -dot(directionToLight, currentVertex.n);
        float lightSamplePdf = triangle.emitterPdf * (distanceToLight * distanceToLight) / (triangle.area * cosTheta);
        float weight = (currentRay.misPdf > 0.0f) ? powerHeuristic(currentRay.misPdf, lightSamplePdf) : 1.0f;
    #elif (IS_MODE == IS_MODE_BSDF)
        float weight = 1.0;
    #else
        float weight = 0.0f;
    #endif

        weight = mix(weight, 1.0, float(currentRay.bounces == 0));
        weight *= float(dot(currentRay.base.direction, currentVertex.n) < 0.0f);

        currentRay.radiance += material.emissive * currentRay.throughput * weight;
    }
#endif

    // sample light
    if (appData.emitterTrianglesCount > 0)
    {
        device const EmitterTriangle& emitterTriangle =
            sampleEmitterTriangle(emitterTriangles, appData.emitterTrianglesCount, noiseSample.w);

        float3 lightTriangleBarycentric = barycentric(noiseSample.yz);
        Vertex lightVertex = interpolate(emitterTriangle.v0, emitterTriangle.v1, emitterTriangle.v2, lightTriangleBarycentric);

        packed_float3 directionToLight = lightVertex.v - currentVertex.v;
        float distanceToLight = length(directionToLight);
        directionToLight /= distanceToLight;

        float DdotN = dot(directionToLight, currentVertex.n);
        float DdotLN = -dot(directionToLight, lightVertex.n);
        float lightSamplePdf = emitterTriangle.pdf * (distanceToLight * distanceToLight) /
            (emitterTriangle.area * DdotLN);

        SampledMaterialProperties materialSample = sampleMaterialProperties(material, currentVertex.n,
            currentRay.base.direction, directionToLight);

    #if (IS_MODE == IS_MODE_MIS)
        float weightDiffuse = powerHeuristic(lightSamplePdf, materialSample.pdfDiffuse);
        float weightSpecular = powerHeuristic(lightSamplePdf, materialSample.pdfSpecular);
        float weightTransmittance = powerHeuristic(lightSamplePdf, materialSample.pdfTransmittance);
    #elif (IS_MODE == IS_MODE_LIGHT)
        float weightDiffuse = 1.0f;
        float weightSpecular = 1.0f;
        float weightTransmittance = 1.0f;
    #else
        float weightDiffuse = 0.0f;
        float weightSpecular = 0.0f;
        float weightTransmittance = 0.0f;
    #endif

        float3 weightedBsdf =
            materialSample.bsdfDiffuse * weightDiffuse +
            materialSample.bsdfSpecular * weightSpecular +
            materialSample.bsdfTransmittance * weightTransmittance;

        bool triangleSampled = (DdotN > 0.0f) && (DdotLN > 0.0f) && (dot(weightedBsdf, weightedBsdf) > 0.0f);

        lightSamplingRay.base.origin = currentVertex.v + currentVertex.n * DISTANCE_EPSILON;
        lightSamplingRay.base.direction = directionToLight;
        lightSamplingRay.base.minDistance = 0.0f;
        lightSamplingRay.base.maxDistance = triangleSampled ? INFINITY : -1.0;
        lightSamplingRay.targetPrimitiveIndex = emitterTriangle.globalIndex;

        lightSamplingRay.throughput = currentRay.throughput *
            (emitterTriangle.emissive / lightSamplePdf) * weightedBsdf;
    }

    // generate next bounce
    {
        SampledMaterial materialSample = sampleMaterial(material, currentVertex.n, currentRay.base.direction, noiseSample);
        currentRay.base.origin = currentVertex.v + materialSample.direction * DISTANCE_EPSILON;
        currentRay.base.direction = materialSample.direction;
        currentRay.misPdf = materialSample.misPdf;
        currentRay.throughput *= materialSample.weight;
        currentRay.bounces += 1;
    }
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
    #if (ADD_RADIANCE)
        rays[rayIndex].radiance += lightSamplingRays[rayIndex].throughput;
    #endif
    }
}
