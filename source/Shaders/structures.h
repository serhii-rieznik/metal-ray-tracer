//
//  structures.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <MetalPerformanceShaders/MetalPerformanceShaders.h>

#if !defined(__METAL_VERSION__)
using packed_float3 = MPSPackedFloat3;
#endif

using Ray = MPSRayOriginMinDistanceDirectionMaxDistance;
using Intersection = MPSIntersectionDistancePrimitiveIndexCoordinates;

struct Vertex
{
    packed_float3 v;
    packed_float3 n;
    packed_float2 t;
};
