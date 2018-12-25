//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "diffuse.h"
#include "conductor.h"
#include "plastic.h"
#include "dielectric.h"

inline SampledMaterial evaluateMaterial(device const Material& material, float3 nO, float3 wI, float3 wO, float wavelength)
{
    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
            return diffuse::evaluate(material, nO, wI, wO, wavelength);
        case MATERIAL_CONDUCTOR:
            return conductor::evaluate(material, nO, wI, wO, wavelength);
        case MATERIAL_PLASTIC:
            return plastic::evaluate(material, nO, wI, wO, wavelength);
        case MATERIAL_DIELECTRIC:
            return dielectric::evaluate(material, nO, wI, wO, wavelength);

        default:
            return {};
    }
}

inline SampledMaterial sampleMaterial(device const Material& material, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
            return diffuse::sample(material, nO, wI, randomSample, wavelength);
        case MATERIAL_CONDUCTOR:
            return conductor::sample(material, nO, wI, randomSample, wavelength);
        case MATERIAL_PLASTIC:
            return plastic::sample(material, nO, wI, randomSample, wavelength);
        case MATERIAL_DIELECTRIC:
            return dielectric::sample(material, nO, wI, randomSample, wavelength);

        default:
            return {};
    }
}
