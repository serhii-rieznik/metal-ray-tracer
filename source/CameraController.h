#pragma once

#import <AppKit/AppKit.h>

struct Camera;

@interface CameraController : NSObject

- (void)handleKeyDown:(uint32_t)key;
- (void)handleKeyUp:(uint32_t)key;

- (BOOL)process:(nonnull struct Camera*)camera;

@end
