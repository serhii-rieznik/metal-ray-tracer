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
#import "TextureProvider.h"
#import "Shaders/structures.h"
#import <random>
#import <map>

static const NSUInteger MaxFrames = 3;

@interface Renderer()

- (id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName;

- (void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                   setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock;

- (void)initializeRayTracing;
- (void)updateBuffers;

@end

@implementation Renderer
{
    dispatch_semaphore_t _frameSemaphore;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLLibrary> _defaultLibrary;
    id<MTLTexture> _outputImage;
    id<MTLTexture> _environmentMap;
    id<MTLTexture> _referenceImage;
    id<MTLRenderPipelineState> _blitPipelineState;
    id<MTLBuffer> _rayBuffer;
    id<MTLBuffer> _lightSamplingRayBuffer;
    id<MTLBuffer> _intersectionBuffer;
    id<MTLBuffer> _lightSamplingIntersectionBuffer;
    id<MTLComputePipelineState> _rayGenerator;
    id<MTLComputePipelineState> _intersectionHandler;
    id<MTLComputePipelineState> _lightSamplingGenerator;
    id<MTLComputePipelineState> _lightSamplingHandler;
    id<MTLComputePipelineState> _accumulation;
    id<MTLBuffer> _noise[MaxFrames];
    id<MTLBuffer> _appData[MaxFrames];

    MPSTriangleAccelerationStructure* _accelerationStructure;
    MPSRayIntersector* _rayIntersector;
    MPSRayIntersector* _lightIntersector;

    GeometryProvider _geometryProvider;
    MTLSize _outputImageSize;
    uint32_t _rayCount;
    uint32_t _frameContinuousIndex;
    CFTimeInterval _startupTime;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _lastFrameDuration;
    bool _hasEmitters;
}

static std::mt19937 randomGenerator;
static std::uniform_real_distribution<float> uniformFloatDistribution(0.0f, 1.0f);

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView*)view
{
    randomGenerator.seed(std::random_device()());

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

        _rayGenerator = [self newComputePipelineWithFunctionName:@"generateRays"];
        _intersectionHandler = [self newComputePipelineWithFunctionName:@"handleIntersections"];
        _lightSamplingGenerator = [self newComputePipelineWithFunctionName:@"generateLightSamplingRays"];
        _lightSamplingHandler = [self newComputePipelineWithFunctionName:@"lightSamplingHandler"];
        _accumulation = [self newComputePipelineWithFunctionName:@"accumulateImage"];

        NSUInteger noiseBufferLength = NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE * sizeof(RandomSample);
        for (uint32_t i = 0; i < MaxFrames; ++i)
        {
            _noise[i] = [_device newBufferWithLength:noiseBufferLength options:MTLResourceStorageModeManaged];
            _appData[i] = [_device newBufferWithLength:sizeof(ApplicationData) options:MTLResourceStorageModeShared];
        }
        [self initializeRayTracing];

        _startupTime = CACurrentMediaTime();
    }

    return self;
}

- (uint32_t)frameIndex
{
    return _frameContinuousIndex % MaxFrames;
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    bool cap = false;

    if (_frameContinuousIndex > 110000)
    {
        [[MTLCaptureManager sharedCaptureManager] startCaptureWithDevice:_device];
        cap = true;
    }

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

    __block dispatch_semaphore_t block_sema = _frameSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        dispatch_semaphore_signal(block_sema);
    }];

    if (_lastFrameTime < CFTimeInterval(MAX_RUN_TIME_IN_SECONDS))
    {
        [self updateBuffers];

        /*
         * Generate rays
         */
        [self dispatchComputeShader:_rayGenerator withinCommandBuffer:commandBuffer setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
            [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
            [commandEncoder setBuffer:self->_noise[[self frameIndex]] offset:0 atIndex:1];
            [commandEncoder setBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:2];
        }];

        // Intersect rays with triangles inside acceleration structure
        [_rayIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                          intersectionType:MPSIntersectionTypeNearest
                                                 rayBuffer:_rayBuffer rayBufferOffset:0
                                        intersectionBuffer:_intersectionBuffer intersectionBufferOffset:0
                                                  rayCount:_rayCount accelerationStructure:_accelerationStructure];

        // Handle intersections, generate light sampling rays, generate next bounce
        [self dispatchComputeShader:_intersectionHandler withinCommandBuffer:commandBuffer
                         setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
         {
             [commandEncoder setTexture:self->_environmentMap atIndex:0];
             [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:0];
             [commandEncoder setBuffer:self->_geometryProvider.materialBuffer() offset:0 atIndex:1];
             [commandEncoder setBuffer:self->_geometryProvider.triangleBuffer() offset:0 atIndex:2];
             [commandEncoder setBuffer:self->_geometryProvider.emitterTriangleBuffer() offset:0 atIndex:3];
             [commandEncoder setBuffer:self->_geometryProvider.vertexBuffer() offset:0 atIndex:4];
             [commandEncoder setBuffer:self->_geometryProvider.indexBuffer() offset:0 atIndex:5];
             [commandEncoder setBuffer:self->_noise[[self frameIndex]] offset:0 atIndex:6];
             [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:7];
             [commandEncoder setBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:8];
         }];

#   if (IS_MODE != IS_MODE_BSDF)
        if (_hasEmitters)
        {
            // generate light sampling rays
            [self dispatchComputeShader:_lightSamplingGenerator withinCommandBuffer:commandBuffer
                             setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
             {
                 [commandEncoder setTexture:self->_environmentMap atIndex:0];
                 [commandEncoder setBuffer:self->_intersectionBuffer offset:0 atIndex:0];
                 [commandEncoder setBuffer:self->_geometryProvider.materialBuffer() offset:0 atIndex:1];
                 [commandEncoder setBuffer:self->_geometryProvider.triangleBuffer() offset:0 atIndex:2];
                 [commandEncoder setBuffer:self->_geometryProvider.emitterTriangleBuffer() offset:0 atIndex:3];
                 [commandEncoder setBuffer:self->_geometryProvider.vertexBuffer() offset:0 atIndex:4];
                 [commandEncoder setBuffer:self->_geometryProvider.indexBuffer() offset:0 atIndex:5];
                 [commandEncoder setBuffer:self->_noise[[self frameIndex]] offset:0 atIndex:6];
                 [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:7];
                 [commandEncoder setBuffer:self->_lightSamplingRayBuffer offset:0 atIndex:8];
                 [commandEncoder setBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:9];
             }];

            // Intersect light sampling rays with geometry
            [_lightIntersector encodeIntersectionToCommandBuffer:commandBuffer
                                                intersectionType:MPSIntersectionTypeNearest
                                                       rayBuffer:_lightSamplingRayBuffer
                                                 rayBufferOffset:0
                                              intersectionBuffer:_lightSamplingIntersectionBuffer
                                        intersectionBufferOffset:0
                                                        rayCount:_rayCount
                                           accelerationStructure:_accelerationStructure];

            // Handle light sampling intersections
            [self dispatchComputeShader:_lightSamplingHandler withinCommandBuffer:commandBuffer
                             setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
                                 [commandEncoder setBuffer:self->_lightSamplingIntersectionBuffer offset:0 atIndex:0];
                                 [commandEncoder setBuffer:self->_lightSamplingRayBuffer offset:0 atIndex:1];
                                 [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:2];
                             }];
        }
#   endif

        /*
         * Accumulate image
         */
        [self dispatchComputeShader:_accumulation withinCommandBuffer:commandBuffer
                         setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder) {
                             [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
                             [commandEncoder setBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:1];
                             [commandEncoder setTexture:self->_outputImage atIndex:0];
                         }];

        if (_frameContinuousIndex % MaxFrames == 0)
        {
            id<MTLRenderCommandEncoder> blitEncoder = [commandBuffer renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];
            [blitEncoder setRenderPipelineState:_blitPipelineState];
            [blitEncoder setFragmentTexture:_outputImage atIndex:0];
            [blitEncoder setFragmentTexture:_referenceImage atIndex:1];
            [blitEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [blitEncoder endEncoding];

            [commandBuffer presentDrawable:view.currentDrawable];

            [[NSApp mainWindow] setTitle:[NSString stringWithFormat:@"Frame: %u (%.2fms. last frame, %.2fs. elapsed)",
                _frameContinuousIndex, _lastFrameDuration * 1000.0f, _lastFrameTime]];
        }
    }

    [commandBuffer commit];

    if (cap)
    {
        [[MTLCaptureManager sharedCaptureManager] stopCapture];
    }
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)inSize
{
    _frameContinuousIndex = 0;

    _outputImageSize.width = inSize.width / CONTENT_SCALE;
    _outputImageSize.height = inSize.height / CONTENT_SCALE;
    _outputImageSize.depth = 1.0f;

    MTLTextureDescriptor* outputImageDescriptor = [MTLTextureDescriptor new];
    outputImageDescriptor.pixelFormat = MTLPixelFormatRGBA32Float;
    outputImageDescriptor.width = NSUInteger(_outputImageSize.width);
    outputImageDescriptor.height = NSUInteger(_outputImageSize.height);
    outputImageDescriptor.usage |= MTLTextureUsageShaderWrite;
    outputImageDescriptor.storageMode = MTLStorageModePrivate;
    _outputImage = [_device newTextureWithDescriptor:outputImageDescriptor];

    _rayCount = uint32_t(_outputImageSize.width) * uint32_t(_outputImageSize.height);

    _rayBuffer = [_device newBufferWithLength:sizeof(Ray) * _rayCount options:MTLResourceStorageModePrivate];
    _lightSamplingRayBuffer = [_device newBufferWithLength:sizeof(LightSamplingRay) * _rayCount
                                                   options:MTLResourceStorageModePrivate];

    _intersectionBuffer = [_device newBufferWithLength:sizeof(Intersection) * _rayCount options:MTLResourceStorageModePrivate];
    _lightSamplingIntersectionBuffer = [_device newBufferWithLength:sizeof(LightSamplingIntersection) * _rayCount
                                                            options:MTLResourceStorageModePrivate];
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
    const std::map<uint32_t, std::pair<NSString*, NSString*>> sceneNames =
    {
        { SCENE_VEACH_MIS, { @"media/veach_mis", @"media/reference/veach_mis" } },
        { SCENE_SPHERE, { @"media/sphere", @"media/reference/sphere" } },
        { SCENE_PBS_SPHERES, { @"media/spheres", @"media/reference/spheres" } },
        { SCENE_CORNELL_BOX_DIFFUSE, { @"media/cornellbox-diffuse", @"media/reference/cornellbox-diffuse" } },
        { SCENE_CORNELL_BOX_CONDUCTOR, { @"media/cornellbox-conductor", @"media/reference/cornellbox-conductor" } },
        { SCENE_CORNELL_BOX_PLASTIC, { @"media/cornellbox-plastic", @"media/reference/cornellbox-plastic" } },
        { SCENE_CORNELL_BOX_DIELECTRIC, { @"media/cornellbox-dielectric", @"media/reference/cornellbox-dielectric" } },
        { SCENE_CORNELL_BOX_SPHERES, { @"media/cornellbox-water-spheres", @"media/reference/cornellbox-water-spheres" } },
    };

    uint32_t sceneId = SCENE;
    {
        NSURL* modelUrl = [[NSBundle mainBundle] URLForResource:sceneNames.at(sceneId).first withExtension:@"obj"];
        const char* fileName = [modelUrl fileSystemRepresentation];
        _geometryProvider.loadFile(fileName, _device);
    }

#if (COMPARE_TO_REFERENCE)
    NSURL* referenceUrl = [[NSBundle mainBundle] URLForResource:sceneNames.at(sceneId).second withExtension:@"exr"];
    if (referenceUrl != nil)
    {
        const char* fileName = [referenceUrl fileSystemRepresentation];
        _referenceImage = TextureProvider::loadFile(fileName, _device);
    }
#endif

    if (_geometryProvider.environment().textureName.empty() == false)
    {
        NSString* textureName = [NSString stringWithFormat:@"media/%s", _geometryProvider.environment().textureName.c_str()];
        textureName = [textureName stringByDeletingPathExtension];
        NSURL* assetUrl = [[NSBundle mainBundle] URLForResource:textureName withExtension:@"exr"];
        if (assetUrl != nil)
        {
            const char* fileName = [assetUrl fileSystemRepresentation];
            _environmentMap = TextureProvider::loadFile(fileName, _device);
        }
    }

    _hasEmitters = _geometryProvider.emitterTriangleCount() > 0;
    
    _accelerationStructure = [[MPSTriangleAccelerationStructure alloc] initWithDevice:_device];
    [_accelerationStructure setVertexBuffer:_geometryProvider.vertexBuffer()];
    [_accelerationStructure setVertexStride:sizeof(Vertex)];
    [_accelerationStructure setIndexBuffer:_geometryProvider.indexBuffer()];
    [_accelerationStructure setIndexType:MPSDataTypeUInt32];
    [_accelerationStructure setTriangleCount:_geometryProvider.triangleCount()];
    [_accelerationStructure rebuild];

    _rayIntersector = [[MPSRayIntersector alloc] initWithDevice:_device];
    [_rayIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
    [_rayIntersector setRayStride:sizeof(Ray)];
    [_rayIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndexCoordinates];
    [_rayIntersector setIntersectionStride:sizeof(Intersection)];
    [_rayIntersector setCullMode:MTLCullModeNone];

    _lightIntersector = [[MPSRayIntersector alloc] initWithDevice:_device];
    [_lightIntersector setRayDataType:MPSRayDataTypeOriginMinDistanceDirectionMaxDistance];
    [_lightIntersector setRayStride:sizeof(LightSamplingRay)];
    [_lightIntersector setIntersectionDataType:MPSIntersectionDataTypeDistancePrimitiveIndex];
    [_lightIntersector setIntersectionStride:sizeof(LightSamplingIntersection)];
    [_lightIntersector setCullMode:MTLCullModeNone];
}

- (void)updateBuffers
{
    RandomSample* ptr = reinterpret_cast<RandomSample*>([_noise[[self frameIndex]] contents]);
    for (NSUInteger i = 0; i < (NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE); ++i)
    {
        ptr->barycentricSample.x = uniformFloatDistribution(randomGenerator);
        ptr->barycentricSample.y = uniformFloatDistribution(randomGenerator);
        ptr->emitterBsdfSample.x = uniformFloatDistribution(randomGenerator);
        ptr->emitterBsdfSample.y = uniformFloatDistribution(randomGenerator);
        ptr->bsdfSample.x = uniformFloatDistribution(randomGenerator);
        ptr->bsdfSample.y = uniformFloatDistribution(randomGenerator);
        ptr->componentSample = uniformFloatDistribution(randomGenerator);
        ptr->emitterSample = uniformFloatDistribution(randomGenerator);
        ptr->rrSample = uniformFloatDistribution(randomGenerator);
        ++ptr;
    }
    [_noise[[self frameIndex]] didModifyRange:NSMakeRange(0, [_noise[[self frameIndex]] length])];

    CFTimeInterval frameTime = CACurrentMediaTime() - _startupTime;
    _lastFrameDuration = frameTime - _lastFrameTime;
    _lastFrameTime = frameTime;

    ApplicationData* appData = reinterpret_cast<ApplicationData*>([_appData[[self frameIndex]] contents]);
    appData->environmentColor = _geometryProvider.environment().uniformColor;
    appData->frameIndex = _frameContinuousIndex;
    appData->emitterTrianglesCount = _geometryProvider.emitterTriangleCount();
    appData->time = frameTime;
    
    ++_frameContinuousIndex;
}

@end
