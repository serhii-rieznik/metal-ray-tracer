//
//  Renderer.m
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import "LightPathTracer.h"

@interface LightPathTracer()
{
    id<MTLBuffer> _rayBuffer;
    id<MTLBuffer> _intersectionBuffer;
    id<MTLComputePipelineState> _rayGenerator;
    id<MTLComputePipelineState> _intersectionHandler;
    id<MTLComputePipelineState> _accumulation;
    MPSRayIntersector* _rayIntersector;
}

@end

@implementation LightPathTracer

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device
{
    if (self = [super initWithGeometryProvider:geometryProvider camera:camera mtkView:mtkView device:device])
    {
        _rayIntersector = [[MPSRayIntersector alloc] initWithDevice:device];
        [_rayIntersector setBoundingBoxIntersectionTestType:MPSBoundingBoxIntersectionTestTypeAxisAligned];
        [_rayIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
        [_rayIntersector setRayStride:sizeof(Ray)];
        [_rayIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndexCoordinates];
        [_rayIntersector setIntersectionStride:sizeof(Intersection)];
        [_rayIntersector setCullMode:MTLCullModeNone];

        _rayGenerator = [self newComputePipelineWithFunctionName:@"lptGenerateRays"];
        _intersectionHandler = [self newComputePipelineWithFunctionName:@"lptHandleIntersections"];
        _accumulation = [self newComputePipelineWithFunctionName:@"lptAccumulateImage"];
    }
    return self;
}

- (void)initResourcesForSize:(CGSize)inSize
{
    [super initResourcesForSize:inSize];
    
    _rayBuffer = [[self device] newBufferWithLength:sizeof(Ray) * [self rayCount]
                                            options:MTLResourceStorageModePrivate];

    _intersectionBuffer = [[self device] newBufferWithLength:sizeof(Intersection) * [self rayCount]
                                                     options:MTLResourceStorageModePrivate];
}

- (void)performPathTracingWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
    [self dispatchComputeShader:_rayGenerator withinCommandBuffer:commandBuffer
                     setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
     {
         [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:0];
         [commandEncoder setBuffer:[self randomSamplesForCurrentFrame] offset:0 atIndex:1];
         [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:2];
         [commandEncoder setBuffer:[self geometryProvider].emitterTriangleBuffer() offset:0 atIndex:3];
     }];

    [_rayIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                      intersectionType:MPSIntersectionTypeNearest
                                             rayBuffer:_rayBuffer rayBufferOffset:0
                                    intersectionBuffer:_intersectionBuffer intersectionBufferOffset:0
                                              rayCount:[self rayCount] accelerationStructure:[self accelerationStructure]];

    // Handle intersections generate next bounce
    [self dispatchComputeShader:_intersectionHandler withinCommandBuffer:commandBuffer
                     setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
     {
         [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:0];
         [commandEncoder setBuffer:[self randomSamplesForCurrentFrame] offset:0 atIndex:1];
         [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:2];
         [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:3];
     }];
    
    [self dispatchComputeShader:_accumulation withinCommandBuffer:commandBuffer
                     setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
     {
         [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:0];
         [commandEncoder setBuffer:[self xyzBuffer] offset:0 atIndex:1];
         [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:2];
         [commandEncoder setTexture:[self outputImage] atIndex:0];
     }];

}

@end
