//
//  RenderView.h
//  MetalRayTracer
//
//  Created by Sergey Reznik on 11/24/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <MetalKit/MetalKit.h>
#import "../CameraController.h"

struct Camera;

NS_ASSUME_NONNULL_BEGIN

@interface RenderView : MTKView

@property (nonatomic, assign) struct Camera* camera;

- (CameraController*)cameraController;
- (void)setFreeCameraMode:(BOOL)enabled;

@end

NS_ASSUME_NONNULL_END
