#import <MetalKit/MetalKit.h>
#import "Shaders/constants.h"
#import "GeometryProvider.h"

@interface PathTracer : NSObject

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device;

- (void)processWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer;

- (void)dispatchComputeShader:(id<MTLComputePipelineState>)pipelineState
          withinCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                   setupBlock:(void(^)(id<MTLComputeCommandEncoder>))setupBlock;

- (id<MTLComputePipelineState>)newComputePipelineWithFunctionName:(NSString*)functionName;

- (void)initResourcesForSize:(CGSize)inSize;
- (void)restart;

- (uint32_t)frameIndex;
- (uint32_t)rayCount;
- (id<MTLBuffer>)xyzBuffer;
- (id<MTLBuffer>)applicationDataForCurrentFrame;
- (id<MTLBuffer>)randomSamplesForCurrentFrame;
- (id<MTLTexture>)outputImage;
- (id<MTLLibrary>)defaultLibrary;
- (GeometryProvider&)geometryProvider;

- (void)performPathTracingWithCommandBuffer:(id<MTLCommandBuffer>)commandBuffer;
- (void)updateApplicationData;

@property (readonly) id<MTLDevice> device;
@property MPSTriangleAccelerationStructure* accelerationStructure;
@property id<MTLTexture> environmentMap;
@property id<MTLTexture> referenceImage;
@property uint32_t comparisonMode;

@end

