//
//  RenderView.m
//  MetalRayTracer
//
//  Created by Sergey Reznik on 11/24/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import "RenderView.h"

@interface RenderView()
{
    CameraController* _cameraController;
    BOOL _freeCameraMode;
}
@end

@implementation RenderView

@synthesize camera;

- (BOOL)acceptsFirstResponder
{
    return _freeCameraMode ? YES : NO;
}

- (void)keyDown:(NSEvent *)event
{
    if (_freeCameraMode)
    {
        [[self cameraController] handleKeyDown:[event keyCode]];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (_freeCameraMode)
    {
        [[self cameraController] handleKeyUp:[event keyCode]];
    }
}

- (void)setFreeCameraMode:(BOOL)enabled
{
    _freeCameraMode = enabled;
}

- (CameraController*)cameraController
{
    if (_cameraController == nil)
    {
        _cameraController = [[CameraController alloc] init];
    }

    return _cameraController;
}

@end
