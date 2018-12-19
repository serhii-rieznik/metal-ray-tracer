//
//  Renderer.m
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import "Renderer.h"
#import "Spectrum.h"
#import "GeometryProvider.h"
#import "TextureProvider.h"
#import "Shaders/structures.h"
#import <random>
#import <map>

static const NSUInteger MaxFrames = 3;
static NSString* kLastOpenedScene = @"last.opened.scene";
static NSString* kGeometry = @"geometry";
static NSString* kReferenceImage = @"reference-image";
static NSString* kCamera = @"camera";
static NSString* kCameraOrigin = @"origin";
static NSString* kCameraTarget = @"target";
static NSString* kCameraUp = @"up";
static NSString* kCameraFOV = @"fov";

@interface Renderer()

- (id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName;

- (void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                   setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock;

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
    id<MTLBuffer> _xyzBuffer;

    MPSTriangleAccelerationStructure* _accelerationStructure;
    MPSRayIntersector* _rayIntersector;
    MPSRayIntersector* _lightIntersector;

    GeometryProvider _geometryProvider;
    Camera _camera;

    MTLSize _outputImageSize;
    uint32_t _rayCount;
    uint32_t _frameContinuousIndex;
    uint32_t _comparisonMode;
    CFTimeInterval _startupTime;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _lastFrameDuration;
    bool _raytracingPaused;
    bool _restartTracing;
}

static std::mt19937 randomGenerator;
static std::uniform_real_distribution<float> uniformFloatDistribution(0.0f, 1.0f);

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView*)view
{
    SampledSpectrum::initialize();
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

        std::vector<packed_float3> xyzData(SpectrumSampleCount);
        for (uint32_t i = 0; i < SpectrumSampleCount; ++i)
        {
            xyzData[i].x = SampledSpectrum::X()[i];
            xyzData[i].y = SampledSpectrum::Y()[i];
            xyzData[i].z = SampledSpectrum::Z()[i];
        }
        BufferConstructor makeBuffer(_device);
        _xyzBuffer = makeBuffer(xyzData, @"XYZ");

        [self initializeRayTracingWithRecent];
    }

    return self;
}

- (uint32_t)frameIndex
{
    return _frameContinuousIndex % MaxFrames;
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 1e+9);
    if (_raytracingPaused || (dispatch_semaphore_wait(_frameSemaphore, timeout) != 0))
        return;

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        dispatch_semaphore_signal(self->_frameSemaphore);
    }];

    if (_restartTracing)
    {
        _startupTime = CACurrentMediaTime();
        _frameContinuousIndex = 0;

        for (uint32_t i = 0; i < MaxFrames; ++i)
            [self updateBuffers];

        _restartTracing = false;
    }
    else if (_lastFrameTime < CFTimeInterval(MAX_RUN_TIME_IN_SECONDS))
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

#   if ((MAX_PATH_LENGTH > 1) && (IS_MODE != IS_MODE_BSDF))
        if (_geometryProvider.emitterTriangleCount() > 0)
        {
            // Generate light sampling rays
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

        /*
         * Accumulate image
         */
        [self dispatchComputeShader:_accumulation withinCommandBuffer:commandBuffer
                         setupBlock:^(id<MTLComputeCommandEncoder> commandEncoder)
         {
             [commandEncoder setBuffer:self->_rayBuffer offset:0 atIndex:0];
             [commandEncoder setBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:1];
             [commandEncoder setBuffer:self->_xyzBuffer offset:0 atIndex:2];
             [commandEncoder setTexture:self->_outputImage atIndex:0];
         }];

        // if (_frameContinuousIndex % MaxFrames == 0)
        {
            id<MTLRenderCommandEncoder> blitEncoder = [commandBuffer renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];
            [blitEncoder setFragmentBuffer:self->_appData[[self frameIndex]] offset:0 atIndex:0];
            [blitEncoder setRenderPipelineState:_blitPipelineState];
            [blitEncoder setFragmentTexture:_outputImage atIndex:0];
            [blitEncoder setFragmentTexture:_referenceImage atIndex:1];
            [blitEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [blitEncoder endEncoding];

            [commandBuffer presentDrawable:view.currentDrawable];

            [[NSApp mainWindow] setTitle:[NSString stringWithFormat:@"Frame: %u (%.2fms. last frame, %.2fs. elapsed)",
                                          _frameContinuousIndex, _lastFrameDuration * 1000.0f, _lastFrameTime]];
        }

        ++_frameContinuousIndex;
    }

    [commandBuffer commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)inSize
{
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

    _restartTracing = true;
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
    [commandEncoder dispatchThreads:_outputImageSize threadsPerThreadgroup:MTLSizeMake(16, 4, 1)];
    [commandEncoder endEncoding];
}

- (void)initializeRayTracingWithRecent
{
    NSString* lastOpenedScene = [[NSUserDefaults standardUserDefaults] stringForKey:kLastOpenedScene];

    if (lastOpenedScene == nil)
    {
        lastOpenedScene = [[NSBundle mainBundle] pathForResource:@"media/cornellbox" ofType:@"json"];
    }

    [self initializeRayTracingWithFile:lastOpenedScene];
}

- (void)initializeRayTracingWithFile:(NSString*)fileName
{
    _raytracingPaused = true;
    _restartTracing = true;

    bool hasCustomCamera = false;
    NSString* geometryFile = fileName;
    NSString* referenceFile = nil;
    if ([[fileName pathExtension] isEqualToString:@"json"])
    {
        NSError* error = nil;
        NSData* jsonData = [NSData dataWithContentsOfFile:fileName];
        NSDictionary* options = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];

        if (error)
        {
            NSLog(@"Failed to load scene from JSON:\n%@", error);
            return;
        }

        geometryFile = [options objectForKey:kGeometry];
        referenceFile = [options objectForKey:kReferenceImage];
        if (NSDictionary* camera = [options objectForKey:kCamera])
        {
            NSArray* origin = [camera objectForKey:kCameraOrigin];
            if (origin != nil)
            {
            _camera.fov = [[camera objectForKey:kCameraFOV] floatValue] * M_PI / 180.0f;
            _camera.origin.x = [[origin objectAtIndex:0] floatValue];
            _camera.origin.y = [[origin objectAtIndex:1] floatValue];
            _camera.origin.z = [[origin objectAtIndex:2] floatValue];
                hasCustomCamera = true;
            }

            NSArray* target = [camera objectForKey:kCameraTarget];
            if (target != nil)
            {
            _camera.target.x = [[target objectAtIndex:0] floatValue];
            _camera.target.y = [[target objectAtIndex:1] floatValue];
            _camera.target.z = [[target objectAtIndex:2] floatValue];
            }

            NSArray* up = [camera objectForKey:kCameraUp];
            if (up != nil)
            {
                _camera.up.x = [[up objectAtIndex:0] floatValue];
                _camera.up.y = [[up objectAtIndex:1] floatValue];
                _camera.up.z = [[up objectAtIndex:2] floatValue];
            }
            else
            {
                _camera.up = { 0.0f, 1.0f, 0.0f };
            }
        }
    }

    if ([[geometryFile pathComponents] count] == 1)
    {
        NSString* folder = [fileName stringByDeletingLastPathComponent];
        geometryFile = [NSString stringWithFormat:@"%@/%@", folder, geometryFile];
    }

    _geometryProvider = GeometryProvider([geometryFile fileSystemRepresentation], _device);

    if (_geometryProvider.triangleCount() == 0)
        return;

    if ([[referenceFile pathComponents] count] == 1)
    {
        NSString* folder = [fileName stringByDeletingLastPathComponent];
        referenceFile = [NSString stringWithFormat:@"%@/%@", folder, referenceFile];
    }

    _referenceImage = TextureProvider::loadFile([referenceFile fileSystemRepresentation], _device);

    _environmentMap = nil;
    if (_geometryProvider.environment().textureName.empty() == false)
    {
        NSString* textureName = [NSString stringWithFormat:@"%s", _geometryProvider.environment().textureName.c_str()];
        textureName = [[geometryFile stringByDeletingLastPathComponent] stringByAppendingPathComponent:textureName];
        _environmentMap = TextureProvider::loadFile([textureName fileSystemRepresentation], _device);
    }

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

    [[NSUserDefaults standardUserDefaults] setObject:fileName forKey:kLastOpenedScene];
    [[NSUserDefaults standardUserDefaults] synchronize];

    if (hasCustomCamera == false)
    {
        packed_float3 bmin = _geometryProvider.boundsMin();
        packed_float3 bmax = _geometryProvider.boundsMax();
        packed_float3 maxv = simd_max(simd_abs(bmin), simd_abs(bmax));
        _camera.target = (bmin + bmax) * 0.5f;
        _camera.origin = { _camera.target.x, _camera.target.y, 2.0f * maxv.z };
    }
    _raytracingPaused = false;
}

- (void)updateBuffers
{
    id<MTLBuffer> noiseBuffer = _noise[[self frameIndex]];
    RandomSample* ptr = reinterpret_cast<RandomSample*>([noiseBuffer contents]);
    for (NSUInteger i = 0; i < (NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE); ++i)
    {
        ptr->pixelSample.x = uniformFloatDistribution(randomGenerator);
        ptr->pixelSample.y = uniformFloatDistribution(randomGenerator);
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
    [_noise[[self frameIndex]] didModifyRange:NSMakeRange(0, [noiseBuffer length])];

    CFTimeInterval frameTime = CACurrentMediaTime() - _startupTime;
    _lastFrameDuration = frameTime - _lastFrameTime;
    _lastFrameTime = frameTime;

    id<MTLBuffer> buffer = _appData[[self frameIndex]];
    ApplicationData* appData = reinterpret_cast<ApplicationData*>([buffer contents]);

    appData->environmentColor = SampledSpectrum::fromRGB(RGBToSpectrumClass::Illuminant,
        _geometryProvider.environment().uniformColor.elements).toGPUSpectrum();

    appData->emitterTrianglesCount = _geometryProvider.emitterTriangleCount();
    appData->frameIndex = _frameContinuousIndex;
    appData->time = frameTime;
    appData->comparisonMode = _comparisonMode;
    appData->camera = _camera;
}

- (void)setComparisonMode:(uint32_t)mode
{
    _comparisonMode = mode;
}

@end
