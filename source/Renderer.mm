//
//  Renderer.m
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import "Renderer.h"
#import "GeometryProvider.h"
#import "Shaders/structures.h"

static const NSUInteger MaxFrames = 3;

@interface Renderer()

-(id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName;

-(void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                  setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock;

-(void)initializeRayTracing;

@end

@implementation Renderer
{
    dispatch_semaphore_t _frameSemaphore;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLLibrary> _defaultLibrary;
    id<MTLTexture> _outputImage;
    id<MTLRenderPipelineState> _blitPipelineState;
    id<MTLComputePipelineState> _testComputeShader;
    id<MTLBuffer> _rayBuffer;
    id<MTLBuffer> _intersectionBuffer;
    id<MTLComputePipelineState> _rayGenerator;
    id<MTLComputePipelineState> _intersectionHandler;

    MPSTriangleAccelerationStructure* _accelerationStructure;
    MPSRayIntersector* _rayIntersector;

    GeometryProvider _geometryProvider;
    MTLSize _outputImageSize;
    uint32_t _rayCount;
}

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView*)view
{
    self = [super init];
    if (self)
    {
        _device = view.device;
        _frameSemaphore = dispatch_semaphore_create(MaxFrames);
        _commandQueue = [_device newCommandQueue];
        _defaultLibrary = [_device newDefaultLibrary];

        /*
         * Create blit render pipeline state
         * which outputs ray-tracing result to the screen
         */
        {
            NSError* error = nil;
            MTLRenderPipelineDescriptor* blitPipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            blitPipelineDescriptor.vertexFunction = [_defaultLibrary newFunctionWithName:@"blitVertex"];
            blitPipelineDescriptor.fragmentFunction = [_defaultLibrary newFunctionWithName:@"blitFragment"];
            blitPipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
            blitPipelineDescriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
            _blitPipelineState = [_device newRenderPipelineStateWithDescriptor:blitPipelineDescriptor error:&error];
        }

        _testComputeShader = [self newComputePipelineWithFunctionName:@"imageFillTest"];
        _rayGenerator = [self newComputePipelineWithFunctionName:@"generateRays"];
        _intersectionHandler = [self newComputePipelineWithFunctionName:@"handleIntersections"];

        [self initializeRayTracing];
    }

    return self;
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

    __block dispatch_semaphore_t block_sema = _frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        dispatch_semaphore_signal(block_sema);
    }];

    /*
     * Dispatch test encoder
     */
    [self dispatchComputeShader:_testComputeShader withinCommandBuffer:commandBuffer setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
        [commandEncoder setTexture:self->_outputImage atIndex:0];
    }];

    /*
     * Generate rays
     */
    [self dispatchComputeShader:_rayGenerator withinCommandBuffer:commandBuffer setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
        [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
    }];

    /*
     * Intersect rays with triangles inside acceleration structure
     */
    [_rayIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                      intersectionType:MPSIntersectionTypeNearest
                                             rayBuffer:_rayBuffer rayBufferOffset:0
                                    intersectionBuffer:_intersectionBuffer intersectionBufferOffset:0
                                              rayCount:_rayCount accelerationStructure:_accelerationStructure];

    /*
     * Handle intersections
     */
    [self dispatchComputeShader:_intersectionHandler withinCommandBuffer:commandBuffer setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
        [commandEncoder setTexture:self->_outputImage atIndex:0];
        [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:0];
    }];

    /*
     * Blit output image to the drawable's textures
     */
    {
        id<MTLRenderCommandEncoder> blitEncoder = [commandBuffer renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];
        [blitEncoder setRenderPipelineState:_blitPipelineState];
        [blitEncoder setFragmentTexture:_outputImage atIndex:0];
        [blitEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [blitEncoder endEncoding];
    }

    [commandBuffer presentDrawable:view.currentDrawable];
    [commandBuffer commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    _outputImageSize.width = size.width;
    _outputImageSize.height = size.height;
    _outputImageSize.depth = 1.0f;

    MTLTextureDescriptor* outputImageDescriptor = [MTLTextureDescriptor new];
    outputImageDescriptor.pixelFormat = MTLPixelFormatRGBA32Float;
    outputImageDescriptor.width = NSUInteger(size.width);
    outputImageDescriptor.height = NSUInteger(size.height);
    outputImageDescriptor.usage |= MTLTextureUsageShaderWrite;
    outputImageDescriptor.storageMode = MTLStorageModePrivate;
    _outputImage = [_device newTextureWithDescriptor:outputImageDescriptor];

    _rayCount = uint32_t(size.width) * uint32_t(size.height);
    _rayBuffer = [_device newBufferWithLength:sizeof(Ray) * _rayCount options:MTLResourceStorageModePrivate];
    _intersectionBuffer = [_device newBufferWithLength:sizeof(Intersection) * _rayCount options:MTLResourceStorageModePrivate];
}

/*
 * Helper functions
 */
#pragma mark - Helper functions

- (id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName
{
    NSError* error = nil;
    id<MTLFunction> function = [_defaultLibrary newFunctionWithName:functionName];
    id<MTLComputePipelineState> result = [_device newComputePipelineStateWithFunction:function error:&error];
    if (error != nil)
    {
        NSLog(@"Failed to create compute pipeline state with function %@", functionName);
        NSLog(@"%@", error);
    }
    return result;
}

-(void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                  setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock
{
    id<MTLComputeCommandEncoder> commandEncoder = [commandBuffer computeCommandEncoder];
    setupBlock(commandEncoder);
    [commandEncoder setComputePipelineState:pipelineState];
    [commandEncoder dispatchThreads:_outputImageSize threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [commandEncoder endEncoding];
}

-(void)initializeRayTracing
{
    _geometryProvider.loadFile("onotole.obj", _device);
    
    _accelerationStructure = [[MPSTriangleAccelerationStructure alloc] initWithDevice:_device];
    [_accelerationStructure setVertexBuffer:_geometryProvider.getVertexBuffer()];
    [_accelerationStructure setVertexStride:sizeof(Vertex)];
    [_accelerationStructure setIndexBuffer:_geometryProvider.getIndexBuffer()];
    [_accelerationStructure setIndexType:MPSDataTypeUInt32];
    [_accelerationStructure setTriangleCount:1];
    [_accelerationStructure rebuild];

    _rayIntersector = [[MPSRayIntersector alloc] initWithDevice:_device];
    [_rayIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
    [_rayIntersector setRayStride:sizeof(Ray)];
    [_rayIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndexCoordinates];
    [_rayIntersector setIntersectionStride:sizeof(Intersection)];
}

@end
