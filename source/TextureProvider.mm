//
//  GeometryProvider.mm
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/17/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include "GeometryProvider.h"
#include "../external/tinyexr/tinyexr.h"

namespace TextureProvider
{

id<MTLTexture> loadFile(const std::string& fileName, id<MTLDevice> device)
{
    union
    {
        char* error = nullptr;
        const char* constError;
    };

    float* rgbaData = nullptr;
    int width = 0;
    int height = 0;

    int loadResult = LoadEXR(&rgbaData, &width, &height, fileName.c_str(), &constError);
    if (loadResult != TINYEXR_SUCCESS)
    {
        if (error != nullptr)
        {
            NSLog(@"Failed to load EXR file: %s", fileName.c_str());
            NSLog(@"Error: %s", error);
            free(error);
            return nil;
        }
    }

    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
        width:width height:height mipmapped:NO];

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    id<MTLTexture> result = [device newTextureWithDescriptor:desc];
    {
        [result replaceRegion:region mipmapLevel:0 slice:0 withBytes:rgbaData
                  bytesPerRow:width * sizeof(float) * 4
                bytesPerImage:width * height * sizeof(float) * 4];
        free(rgbaData);
    }
    return result;
}

}

