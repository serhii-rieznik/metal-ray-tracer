#import "ViewPathTracer.h"

@interface ViewPathTracer()
{
    id<MTLBuffer> _rayBuffer;
    id<MTLBuffer> _lightSamplingRayBuffer;
    id<MTLBuffer> _intersectionBuffer;
    id<MTLBuffer> _lightSamplingIntersectionBuffer;

    id<MTLComputePipelineState> _rayGenerator;
    id<MTLComputePipelineState> _intersectionHandler;
    id<MTLComputePipelineState> _lightSamplingGenerator;
    id<MTLComputePipelineState> _lightSamplingHandler;
    id<MTLComputePipelineState> _accumulation;

    MPSRayIntersector* _rayIntersector;
    MPSRayIntersector* _lightIntersector;
}

@end

@implementation ViewPathTracer

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device
{
    if (self = [super initWithGeometryProvider:geometryProvider camera:camera mtkView:mtkView device:device])
    {
        _rayIntersector = [[MPSRayIntersector alloc] initWithDevice:device];
        [_rayIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
        [_rayIntersector setRayStride:sizeof(Ray)];
        [_rayIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndexCoordinates];
        [_rayIntersector setIntersectionStride:sizeof(Intersection)];
        [_rayIntersector setCullMode:MTLCullModeNone];

        _lightIntersector = [[MPSRayIntersector alloc] initWithDevice:device];
        [_lightIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
        [_lightIntersector setRayStride:sizeof(LightSamplingRay)];
        [_lightIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndex];
        [_lightIntersector setIntersectionStride:sizeof(LightSamplingIntersection)];
        [_lightIntersector setCullMode:MTLCullModeNone];

        _rayGenerator = [self newComputePipelineWithFunctionName:@"vptGenerateRays"];
        _intersectionHandler = [self newComputePipelineWithFunctionName:@"vptHandleIntersections"];
        _accumulation = [self newComputePipelineWithFunctionName:@"vptAccumulateImage"];
        _lightSamplingGenerator = [self newComputePipelineWithFunctionName:@"generateLightSamplingRays"];
        _lightSamplingHandler = [self newComputePipelineWithFunctionName:@"lightSamplingHandler"];
    }
    return self;
}

- (void)initResourcesForSize:(CGSize)inSize
{
    [super initResourcesForSize:inSize];

    _rayBuffer = [[self device] newBufferWithLength:sizeof(Ray) * [self rayCount]
                                            options:MTLResourceStorageModePrivate];

    _lightSamplingRayBuffer = [[self device] newBufferWithLength:sizeof(LightSamplingRay) * [self rayCount]
                                                         options:MTLResourceStorageModePrivate];

    _intersectionBuffer = [[self device] newBufferWithLength:sizeof(Intersection) * [self rayCount]
                                                     options:MTLResourceStorageModePrivate];

    _lightSamplingIntersectionBuffer = [[self device] newBufferWithLength:sizeof(LightSamplingIntersection) * [self rayCount]
                                                                  options:MTLResourceStorageModePrivate];

}

- (void)performPathTracingWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
    /*
     * Generate rays
     */
    [self dispatchComputeShader:_rayGenerator withinCommandBuffer:commandBuffer setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
        [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
        [commandEncoder setBuffer:[self randomSamplesForCurrentFrame] offset:0 atIndex:1];
        [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:2];
    }];

    // Intersect rays with triangles inside acceleration structure
    [_rayIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                      intersectionType:MPSIntersectionTypeNearest
                                             rayBuffer:_rayBuffer rayBufferOffset:0
                                    intersectionBuffer:_intersectionBuffer intersectionBufferOffset:0
                                              rayCount:[self rayCount] accelerationStructure:[self accelerationStructure]];

#   if ((MAX_PATH_LENGTH > 1) && (IS_MODE != IS_MODE_BSDF))
    if ([self geometryProvider].emitterTriangleCount() > 0)
    {
        // Generate light sampling rays
        [self dispatchComputeShader:_lightSamplingGenerator withinCommandBuffer:commandBuffer
                         setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
         {
             [commandEncoder setTexture:[self environmentMap] atIndex:0];
             [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:0];
             [commandEncoder setBuffer:[self geometryProvider].materialBuffer() offset:0 atIndex:1];
             [commandEncoder setBuffer:[self geometryProvider].triangleBuffer() offset:0 atIndex:2];
             [commandEncoder setBuffer:[self geometryProvider].emitterTriangleBuffer() offset:0 atIndex:3];
             [commandEncoder setBuffer:[self geometryProvider].vertexBuffer() offset:0 atIndex:4];
             [commandEncoder setBuffer:[self geometryProvider].indexBuffer() offset:0 atIndex:5];
             [commandEncoder setBuffer:[self randomSamplesForCurrentFrame] offset:0 atIndex:6];
             [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:7];
             [commandEncoder setBuffer:self->_lightSamplingRayBuffer offset:0 atIndex:8];
             [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:9];
         }];

        // Intersect light sampling rays with geometry
        [_lightIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                            intersectionType:MPSIntersectionTypeNearest
                                                   rayBuffer:_lightSamplingRayBuffer
                                             rayBufferOffset:0
                                          intersectionBuffer:_lightSamplingIntersectionBuffer
                                    intersectionBufferOffset:0
                                                    rayCount:[self rayCount]
                                       accelerationStructure:[self accelerationStructure]];

        // Handle light sampling intersections
        [self dispatchComputeShader:_lightSamplingHandler withinCommandBuffer:commandBuffer
                         setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
         {
             [commandEncoder setBuffer:self->_lightSamplingIntersectionBuffer offset:0 atIndex:0];
             [commandEncoder setBuffer:self->_lightSamplingRayBuffer offset:0 atIndex:1];
             [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:2];
         }];
    }
#   endif

    // Handle intersections generate next bounce
    [self dispatchComputeShader:_intersectionHandler withinCommandBuffer:commandBuffer
                     setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
     {
         [commandEncoder setTexture:[self environmentMap] atIndex:0];
         [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:0];
         [commandEncoder setBuffer:[self geometryProvider].materialBuffer() offset:0 atIndex:1];
         [commandEncoder setBuffer:[self geometryProvider].triangleBuffer() offset:0 atIndex:2];
         [commandEncoder setBuffer:[self geometryProvider].emitterTriangleBuffer() offset:0 atIndex:3];
         [commandEncoder setBuffer:[self geometryProvider].vertexBuffer() offset:0 atIndex:4];
         [commandEncoder setBuffer:[self geometryProvider].indexBuffer() offset:0 atIndex:5];
         [commandEncoder setBuffer:[self randomSamplesForCurrentFrame] offset:0 atIndex:6];
         [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:7];
         [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:8];
     }];

    /*
     * Accumulate image
     */
    [self dispatchComputeShader:_accumulation withinCommandBuffer:commandBuffer
                     setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
     {
         [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
         [commandEncoder setBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:1];
         [commandEncoder setBuffer:[self xyzBuffer] offset:0 atIndex:2];
         [commandEncoder setTexture:[self outputImage] atIndex:0];
     }];
}

@end
