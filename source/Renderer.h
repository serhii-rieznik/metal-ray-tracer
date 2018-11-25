//
//  Renderer.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <MetalKit/MetalKit.h>

@interface Renderer : NSObject <MTKViewDelegate>

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view;

- (void)initializeRayTracingWithFile:(nonnull NSString*)fileName;
- (void)initializeRayTracingWithRecent;

- (void)setComparisonMode:(uint32_t)mode;

@end

