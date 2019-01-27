#import "PathTracer.h"

@interface LightPathTracer : PathTracer

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device;

@end

