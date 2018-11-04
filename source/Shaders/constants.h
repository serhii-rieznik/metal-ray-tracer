//
//  structures.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <MetalPerformanceShaders/MetalPerformanceShaders.h>

#define PI                  3.1415926536
#define DOUBLE_PI           6.2831853072
#define INVERSE_PI          0.3183098862
#define DISTANCE_EPSILON    0.001
#define ANGLE_EPSILON       0.0001523048

#define ENABLE_IMAGE_ACCUMULATION   1

#define MATERIAL_DIFFUSE            0
#define MATERIAL_CONDUCTOR          1
#define MATERIAL_PLASTIC            2
#define MATERIAL_DIELECTRIC         3
#define MATERIAL_EMITTER            1024

#define NOISE_BLOCK_SIZE            128

#define MAX_RUN_TIME_IN_SECONDS     (60 * 60 * 24) // one day
#define MAX_PATH_LENGTH             2
#define MAX_SAMPLES                 0xffffffff

#define CONTENT_SCALE       2

#define IS_MODE_MIS         0
#define IS_MODE_LIGHT       1
#define IS_MODE_BSDF        2
#define IS_MODE             IS_MODE_MIS

#define SCENE_CORNELL_BOX           0
#define SCENE_VEACH_MIS             1
#define SCENE_CORNELL_BOX_SPHERES   2
#define SCENE_SPHERE                3
#define SCENE_PBS_SPHERES           4
#define SCENE                       SCENE_CORNELL_BOX_SPHERES
