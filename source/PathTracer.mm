//
//  Renderer.m
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import "PathTracer.h"
#import <random>

@interface PathTracer()
{
    id<MTLDevice> _device;
    id<MTLTexture> _outputImage;
    id<MTLRenderPipelineState> _blitPipelineState;
    id<MTLBuffer> _appData[MAX_BUFFERED_FRAMES];
    id<MTLBuffer> _noise[MAX_BUFFERED_FRAMES];
    id<MTLBuffer> _xyzBuffer;
    id<MTLLibrary> _defaultLibrary;

    GeometryProvider* _geometryProvider;
    Camera* _camera;
    MTKView* _mtkView;
    MTLSize _outputImageSize;
    uint32_t _frameContinuousIndex;
    uint32_t _rayCount;
    uint32_t _comparisonMode;
    CFTimeInterval _startupTime;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _lastFrameDuration;
    bool _restartTracing;
}
@end

@implementation PathTracer

@synthesize accelerationStructure;
@synthesize environmentMap;
@synthesize referenceImage;
@synthesize comparisonMode;

static std::mt19937 randomGenerator;
static std::uniform_real_distribution<float> uniformFloatDistribution(0.0f, 1.0f);

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device;
{
    if (self = [super init])
    {
        _geometryProvider = geometryProvider;
        _camera = camera;
        _mtkView = mtkView;
        _device = device;

        {
            NSError* error = nil;
            MTLRenderPipelineDescriptor* blitPipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            blitPipelineDescriptor.vertexFunction = [[self defaultLibrary] newFunctionWithName:@"blitVertex"];
            blitPipelineDescriptor.fragmentFunction = [[self defaultLibrary] newFunctionWithName:@"blitFragment"];
            blitPipelineDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
            blitPipelineDescriptor.depthAttachmentPixelFormat = mtkView.depthStencilPixelFormat;
            _blitPipelineState = [_device newRenderPipelineStateWithDescriptor:blitPipelineDescriptor error:&error];
        }

        NSUInteger noiseBufferLength = NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE * sizeof(RandomSample);
        for (uint32_t i = 0; i < MAX_BUFFERED_FRAMES; ++i)
        {
            _noise[i] = [_device newBufferWithLength:noiseBufferLength options:MTLResourceStorageModeManaged];
            _appData[i] = [_device newBufferWithLength:sizeof(ApplicationData) options:MTLResourceStorageModeShared];
        }

        std::vector<packed_float3> xyzData(CIESpectrumSampleCount);
        for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
        {
            xyzData[i].x = CIE::x[i];
            xyzData[i].y = CIE::y[i];
            xyzData[i].z = CIE::z[i];
        }
        BufferConstructor makeBuffer(_device);
        _xyzBuffer = makeBuffer(xyzData, @"XYZ");

        randomGenerator.seed(std::random_device()());
    }
    return self;
}

- (id<MTLLibrary>)defaultLibrary
{
    if (_defaultLibrary == nil)
    {
        _defaultLibrary = [_device newDefaultLibrary];
    }
    return _defaultLibrary;
}

- (void)processWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
    if (_restartTracing)
    {
        _startupTime = CACurrentMediaTime();
        _frameContinuousIndex = 0;

        for (uint32_t i = 0; i < MAX_BUFFERED_FRAMES; ++i)
            [self updateApplicationData];

        _restartTracing = false;
    }
    else if (_lastFrameTime < CFTimeInterval(MAX_RUN_TIME_IN_SECONDS))
    {
        [self updateApplicationData];
        [self performPathTracingWithCommandBuffer:commandBuffer];
        [self blitOutputImageWithCommandBuffer:commandBuffer];
        ++_frameContinuousIndex;
    }
}

- (void)blitOutputImageWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
    if (_frameContinuousIndex % MAX_BUFFERED_FRAMES == 0)
    {
        id<MTLRenderCommandEncoder> blitEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_mtkView.currentRenderPassDescriptor];
        [blitEncoder setFragmentBuffer:[self applicationDataForCurrentFrame] offset:0 atIndex:0];
        [blitEncoder setRenderPipelineState:_blitPipelineState];
        [blitEncoder setFragmentTexture:_outputImage atIndex:0];
        [blitEncoder setFragmentTexture:[self referenceImage] atIndex:1];
        [blitEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [blitEncoder endEncoding];

        [commandBuffer presentDrawable:_mtkView.currentDrawable];

        [[NSApp mainWindow] setTitle:[NSString stringWithFormat:@"Frame: %u (%.2fms. last frame, %.2fs. elapsed)",
                                      _frameContinuousIndex, _lastFrameDuration * 1000.0f, _lastFrameTime]];
    }}

- (void)initResourcesForSize:(CGSize)inSize
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
    [self restart];
}

-(void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState
         withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                  setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock
{
    id<MTLComputeCommandEncoder> commandEncoder = [commandBuffer computeCommandEncoder];
    setupBlock(commandEncoder);
    [commandEncoder setComputePipelineState:pipelineState];
    [commandEncoder dispatchThreads:_outputImageSize threadsPerThreadgroup:MTLSizeMake(16, 4, 1)];
    [commandEncoder endEncoding];
}

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

- (void)restart
{
    _restartTracing = true;
}

- (uint32_t)frameIndex
{
    return _frameContinuousIndex % MAX_BUFFERED_FRAMES;
}

- (uint32_t)rayCount
{
    return _rayCount;
}

- (id<MTLBuffer>)applicationDataForCurrentFrame
{
    return _appData[[self frameIndex]];
}

- (id<MTLBuffer>)randomSamplesForCurrentFrame
{
    return _noise[[self frameIndex]];
}

- (id<MTLBuffer>)xyzBuffer
{
    return _xyzBuffer;
}

- (id<MTLTexture>)outputImage
{
    return _outputImage;
}

- (GeometryProvider&)geometryProvider
{
    return *_geometryProvider;
}

- (id<MTLDevice>)device
{
    return _device;
}

- (void)updateApplicationData
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
        ptr->wavelengthSample = uniformFloatDistribution(randomGenerator);
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
    appData->environmentColor = _geometryProvider->environment().uniformColor;
    appData->emitterTrianglesCount = _geometryProvider->emitterTriangleCount();
    appData->frameIndex = _frameContinuousIndex;
    appData->time = frameTime;
    appData->comparisonMode = _comparisonMode;
    appData->camera = *_camera;
}

- (void)performPathTracingWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
}

@end
