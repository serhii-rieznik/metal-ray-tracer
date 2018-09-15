//
//  Renderer.m
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import "Renderer.h"

static const NSUInteger MaxFrames = 3;

@interface Renderer()

-(id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName;

-(void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                  setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock;

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

    MTLSize _outputImageSize;
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
}

/*
 * Helper functions
 */
#pragma mark - Helper functions

- (id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName
{
    NSError* error = nil;
    id<MTLFunction> function = [_defaultLibrary newFunctionWithName:@"imageFillTest"];
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
    [commandEncoder setComputePipelineState:_testComputeShader];
    [commandEncoder dispatchThreads:_outputImageSize threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [commandEncoder endEncoding];
}

@end
