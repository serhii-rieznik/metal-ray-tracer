#import "PathTracer.h"

@interface ViewPathTracer : PathTracer

- (instancetype)initWithGeometryProvider:(GeometryProvider*)geometryProvider camera:(Camera*)camera
                                 mtkView:(MTKView*)mtkView device:(id<MTLDevice>)device;

@end

