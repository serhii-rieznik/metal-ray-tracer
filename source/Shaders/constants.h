//
//  structures.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <MetalPerformanceShaders/MetalPerformanceShaders.h>

#define PI                          3.1415926536
#define DOUBLE_PI                   6.2831853072
#define INVERSE_PI                  0.3183098862

#define DISTANCE_EPSILON            0.0001111111
#define ANGLE_EPSILON               0.0001523048

#define LUMINANCE_VECTOR            packed_float3(0.2126f, 0.7152f, 0.0722f)

#define ENABLE_IMAGE_ACCUMULATION   1

#define MATERIAL_DIFFUSE            0
#define MATERIAL_CONDUCTOR          1
#define MATERIAL_PLASTIC            2
#define MATERIAL_DIELECTRIC         3
#define MATERIAL_EMITTER            1024

#define NOISE_BLOCK_SIZE            128

#define MAX_RUN_TIME_IN_SECONDS     (60 * 60 * 24) // one day
#define MAX_SAMPLES                 0xffffffff
#define MAX_PATH_LENGTH             0xffffffff

#define COMPARE_DISABLED            0
#define COMPARE_TO_REFERENCE        1
#define COMPARE_TO_GRAY             2
#define COMPARE_MODE                COMPARE_DISABLED
#define COMPARE_SCALE               2.0f

#define CONTENT_SCALE               2

#define IS_MODE_MIS                 0
#define IS_MODE_LIGHT               1
#define IS_MODE_BSDF                2
#define IS_MODE                     IS_MODE_MIS

#define ENABLE_RUSSIAN_ROULETTE     1

/*
 * Scene
 */
#define SCENE_CORNELL_BOX_DIFFUSE       0
#define SCENE_CORNELL_BOX_CONDUCTOR     1
#define SCENE_CORNELL_BOX_PLASTIC       2
#define SCENE_CORNELL_BOX_DIELECTRIC    3
#define SCENE_CORNELL_BOX_SPHERES       4
#define SCENE_CORNELL_BOX_SMALL_LIGHT   5

#define SCENE_VEACH_MIS                 10
#define SCENE_SPHERE                    11
#define SCENE_PBS_SPHERES               12

#define SCENE                           SCENE_CORNELL_BOX_SPHERES

#define SCENE_CORNELL_BOX_MAX           SCENE_CORNELL_BOX_SMALL_LIGHT
