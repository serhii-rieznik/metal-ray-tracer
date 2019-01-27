#include "CameraController.h"
#include "Shaders/structures.h"
#include <map>

@interface CameraController()
{
    packed_float3 target;
    std::map<uint32_t, bool> _pressedKeys;
}
@end

@implementation CameraController

const uint32_t kLeft = 0;
const uint32_t kDown = 1;
const uint32_t kRight = 2;
const uint32_t kUp = 13;

- (void)handleKeyDown:(uint32_t)key
{
    _pressedKeys[key] = true;
}

- (void)handleKeyUp:(uint32_t)key
{
    _pressedKeys[key] = false;
}

- (BOOL)process:(nonnull struct Camera*)camera
{
    assert(camera != nil);

    float directional = 0.0f;

    if (_pressedKeys[kUp])
    {
        directional += 1.0f;
    }

    if (_pressedKeys[kDown])
    {
        directional -= 1.0f;
    }

    float3 direction = simd_normalize(camera->target - camera->origin);
    float3 delta = direction * (directional * 0.1f);

    camera->origin = camera->origin + delta;
    camera->target = camera->target + delta;

    return (directional != 0.0f);
}

@end
