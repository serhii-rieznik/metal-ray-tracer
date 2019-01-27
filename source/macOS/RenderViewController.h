//
//  RenderViewController.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Renderer.h"

@interface RenderViewController : NSViewController

- (IBAction)openFile:(id)sender;
- (IBAction)displayFinalImage:(id)sender;
- (IBAction)compareToReference:(id)sender;
- (IBAction)compareToNeutralColor:(id)sender;
- (IBAction)restartRaytracing:(id)sender;
- (IBAction)freeCameraMode:(id)sender;

@end
