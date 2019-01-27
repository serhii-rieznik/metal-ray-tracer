//
//  RenderViewController.mm
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <MetalKit/MetalKit.h>
#import "RenderViewController.h"
#import "RenderView.h"
#import "Renderer.h"
#import "../Shaders/constants.h"

@implementation RenderViewController
{
    RenderView* _view;
    Renderer* _renderer;
}

- (void)viewDidLoad
{
    [[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:@"running in the background"];

    [super viewDidLoad];

    _view = (RenderView*)self.view;
    _view.layer.contentsScale = [[NSScreen mainScreen] backingScaleFactor];
    _view.device = MTLCreateSystemDefaultDevice();
    _view.depthStencilPixelFormat = MTLPixelFormatInvalid;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.paused = YES;
    _view.enableSetNeedsDisplay = NO;
    _view.preferredFramesPerSecond = 120;

    if (_view.device == nil)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];
    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];
    _view.delegate = _renderer;

    [NSTimer scheduledTimerWithTimeInterval:1.0f repeats:NO block:^(NSTimer * _Nonnull timer) {
        NSLog(@"Launching render loop...");
        [NSTimer scheduledTimerWithTimeInterval:0.0f repeats:YES block:^(NSTimer * _Nonnull timer) {
            [self->_view draw];
        }];
    }];
    NSLog(@"Loading completed");
}
    
- (IBAction)openFile:(id)sender
{
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles:YES];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setResolvesAliases:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setAllowedFileTypes:@[@"obj", @"json"]];
    if ([openPanel runModal] == NSModalResponseOK)
    {
        NSURL* selectedFile = [openPanel URL];
        [_renderer initializeRayTracingWithFile:[selectedFile path]];
    }
}

- (IBAction)displayFinalImage:(id)sender
{
    [_renderer setComparisonMode:COMPARE_DISABLED];
}

- (IBAction)compareToReference:(id)sender
{
    [_renderer setComparisonMode:COMPARE_TO_REFERENCE];
}

- (IBAction)compareToNeutralColor:(id)sender
{
    [_renderer setComparisonMode:COMPARE_TO_GRAY];
}

- (IBAction)restartRaytracing:(id)sender
{
    [_renderer initializeRayTracingWithRecent];
}

- (IBAction)freeCameraMode:(id)sender
{
    [_view setFreeCameraMode:YES];
}

@end
