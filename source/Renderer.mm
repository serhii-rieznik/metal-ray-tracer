#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import "macOS/RenderView.h"
#import "Renderer.h"
#import "Spectrum.h"
#import "GeometryProvider.h"
#import "TextureProvider.h"
#import "Shaders/structures.h"
#import "ViewPathTracer.h"
#import "LightPathTracer.h"
#import <map>

static NSString* kLastOpenedScene = @"last.opened.scene";
static NSString* kGeometry = @"geometry";
static NSString* kReferenceImage = @"reference-image";
static NSString* kCamera = @"camera";
static NSString* kCameraOrigin = @"origin";
static NSString* kCameraTarget = @"target";
static NSString* kCameraUp = @"up";
static NSString* kCameraFOV = @"fov";

@implementation Renderer
{
    dispatch_semaphore_t _frameSemaphore;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLLibrary> _defaultLibrary;
    id<MTLTexture> _environmentMap;
    id<MTLTexture> _referenceImage;
    MPSTriangleAccelerationStructure* _accelerationStructure;

    PathTracer* _pathTracer;
    GeometryProvider _geometryProvider;
    Camera _camera;
    RenderView* _view;

    bool _raytracingPaused;
}

-(nonnull instancetype)initWithMetalKitView:(nonnull RenderView*)view
{
    CIESpectrum::initialize();

    self = [super init];
    if (self)
    {
        _view = view;
        _device = view.device;
        _frameSemaphore = dispatch_semaphore_create(MAX_BUFFERED_FRAMES);
        _commandQueue = [_device newCommandQueue];
        _defaultLibrary = [_device newDefaultLibrary];
        _pathTracer = [[ViewPathTracer alloc] initWithGeometryProvider:&_geometryProvider camera:&_camera mtkView:view device:_device];

        [_view setCamera:&_camera];
        [self initializeRayTracingWithRecent];
    }

    return self;
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 1e+9);
    if (_raytracingPaused || (dispatch_semaphore_wait(_frameSemaphore, timeout) != 0))
        return;

    if ([[_view cameraController] process:&_camera])
    {
        [_pathTracer restart];
    }

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        dispatch_semaphore_signal(self->_frameSemaphore);
    }];

    [_pathTracer processWithCommandBuffer:commandBuffer];
    
    [commandBuffer commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)inSize
{
    [_pathTracer initResourcesForSize:inSize];
}

/*
 * Helper functions
 */
#pragma mark - Helper functions

- (void)initializeRayTracingWithRecent
{
    NSString* lastOpenedScene = [[NSUserDefaults standardUserDefaults] stringForKey:kLastOpenedScene];

    if (lastOpenedScene == nil)
    {
        lastOpenedScene = [[NSBundle mainBundle] pathForResource:@"media/cornellbox" ofType:@"json"];
    }

    [self initializeRayTracingWithFile:lastOpenedScene];
}

- (void)initializeRayTracingWithFile:(NSString*)fileName
{
    _raytracingPaused = true;

    bool hasCustomCamera = false;
    NSString* geometryFile = fileName;
    NSString* referenceFile = nil;
    if ([[fileName pathExtension] isEqualToString:@"json"])
    {
        NSError* error = nil;
        NSData* jsonData = [NSData dataWithContentsOfFile:fileName];
        NSDictionary* options = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];

        if (error)
        {
            NSLog(@"Failed to load scene from JSON:\n%@", error);
            return;
        }

        geometryFile = [options objectForKey:kGeometry];
        referenceFile = [options objectForKey:kReferenceImage];
        if (NSDictionary* camera = [options objectForKey:kCamera])
        {
            NSArray* origin = [camera objectForKey:kCameraOrigin];
            if (origin != nil)
            {
            _camera.fov = [[camera objectForKey:kCameraFOV] floatValue] * M_PI / 180.0f;
            _camera.origin.x = [[origin objectAtIndex:0] floatValue];
            _camera.origin.y = [[origin objectAtIndex:1] floatValue];
            _camera.origin.z = [[origin objectAtIndex:2] floatValue];
                hasCustomCamera = true;
            }

            NSArray* target = [camera objectForKey:kCameraTarget];
            if (target != nil)
            {
            _camera.target.x = [[target objectAtIndex:0] floatValue];
            _camera.target.y = [[target objectAtIndex:1] floatValue];
            _camera.target.z = [[target objectAtIndex:2] floatValue];
            }

            NSArray* up = [camera objectForKey:kCameraUp];
            if (up != nil)
            {
                _camera.up.x = [[up objectAtIndex:0] floatValue];
                _camera.up.y = [[up objectAtIndex:1] floatValue];
                _camera.up.z = [[up objectAtIndex:2] floatValue];
            }
            else
            {
                _camera.up = { 0.0f, 1.0f, 0.0f };
            }
        }
    }

    if ([[geometryFile pathComponents] count] == 1)
    {
        NSString* folder = [fileName stringByDeletingLastPathComponent];
        geometryFile = [NSString stringWithFormat:@"%@/%@", folder, geometryFile];
    }

    _geometryProvider = GeometryProvider([geometryFile fileSystemRepresentation], _device);

    if (_geometryProvider.triangleCount() == 0)
        return;

    if ([[referenceFile pathComponents] count] == 1)
    {
        NSString* folder = [fileName stringByDeletingLastPathComponent];
        referenceFile = [NSString stringWithFormat:@"%@/%@", folder, referenceFile];
    }

    id<MTLTexture> referenceImage = TextureProvider::loadFile([referenceFile fileSystemRepresentation], _device);
    [_pathTracer setReferenceImage:referenceImage];

    _environmentMap = nil;
    if (_geometryProvider.environment().textureName.empty() == false)
    {
        NSString* textureName = [NSString stringWithFormat:@"%s", _geometryProvider.environment().textureName.c_str()];
        textureName = [[geometryFile stringByDeletingLastPathComponent] stringByAppendingPathComponent:textureName];
        _environmentMap = TextureProvider::loadFile([textureName fileSystemRepresentation], _device);
    }
    [_pathTracer setEnvironmentMap:_environmentMap];

    _accelerationStructure = [[MPSTriangleAccelerationStructure alloc] initWithDevice:_device];
    [_accelerationStructure setVertexBuffer:_geometryProvider.vertexBuffer()];
    [_accelerationStructure setVertexStride:sizeof(Vertex)];
    [_accelerationStructure setIndexBuffer:_geometryProvider.indexBuffer()];
    [_accelerationStructure setIndexType:MPSDataTypeUInt32];
    [_accelerationStructure setTriangleCount:_geometryProvider.triangleCount()];
    [_accelerationStructure rebuild];
    
    [_pathTracer setAccelerationStructure:_accelerationStructure];

    [[NSUserDefaults standardUserDefaults] setObject:fileName forKey:kLastOpenedScene];
    [[NSUserDefaults standardUserDefaults] synchronize];

    if (hasCustomCamera == false)
    {
        packed_float3 bmin = _geometryProvider.boundsMin();
        packed_float3 bmax = _geometryProvider.boundsMax();
        packed_float3 maxv = simd_max(simd_abs(bmin), simd_abs(bmax));
        _camera.target = (bmin + bmax) * 0.5f;
        _camera.origin = { _camera.target.x, _camera.target.y, 2.0f * maxv.z };
    }

    [_pathTracer restart];
    _raytracingPaused = false;
}

- (void)setComparisonMode:(uint32_t)mode
{
    [_pathTracer setComparisonMode:mode];
}

@end
