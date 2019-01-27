#import <MetalKit/MetalKit.h>

@class RenderView;

@interface Renderer : NSObject <MTKViewDelegate>

- (nonnull instancetype)initWithMetalKitView:(nonnull RenderView*)view;

- (void)initializeRayTracingWithFile:(nonnull NSString*)fileName;
- (void)initializeRayTracingWithRecent;
- (void)setComparisonMode:(uint32_t)mode;

@end

