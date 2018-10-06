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
#define DISTANCE_EPSILON    0.005
#define ANGLE_EPSILON       0.0001523048

#define MATERIAL_DIFFUSE    0
#define MATERIAL_CONDUCTOR  1
#define MATERIAL_MIRROR     3
#define MATERIAL_LIGHT      100

#define NOISE_BLOCK_SIZE    64

#define MAX_PATH_LENGTH     8
#define MAX_SAMPLES         0xffffffff

#define CONTENT_SCALE       2

#define IS_MODE_MIS         0
#define IS_MODE_LIGHT       1
#define IS_MODE_BSDF        2
#define IS_MODE             IS_MODE_MIS

