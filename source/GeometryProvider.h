#pragma once

#include "Shaders/structures.h"
#include "Spectrum.h"
#include <vector>
#include <string>

struct Environment
{
    GPUSpectrum uniformColor;
    std::string textureName;
};

class BufferConstructor
{
public:
    BufferConstructor(id<MTLDevice> device)
        : _device(device) { }

    template <class T>
    id<MTLBuffer> operator()(const std::vector<T>& data, NSString* tag) {
        id<MTLBuffer> buffer = [_device newBufferWithBytes:data.data() length:sizeof(T) * data.size()
            options:MTLResourceStorageModeManaged];
        [buffer setLabel:tag];
        return buffer;
    };
private:
    id<MTLDevice> _device;
};

class GeometryProvider
{
public:
    GeometryProvider() = default;
    GeometryProvider(const char*, id<MTLDevice>);

    id<MTLBuffer> indexBuffer() const { return _indexBuffer; }
    id<MTLBuffer> vertexBuffer() const { return _vertexBuffer; }
    id<MTLBuffer> materialBuffer() const { return _materialBuffer; }
    id<MTLBuffer> triangleBuffer() const { return _triangleBuffer; }
    id<MTLBuffer> emitterTriangleBuffer() const { return _emitterTriangleBuffer; }

    uint32_t triangleCount() const { return _triangleCount; }
    uint32_t emitterTriangleCount() const { return _emitterTriangleCount; }

    const Environment& environment() const { return _environment; }

    packed_float3 boundsMin() const { return _boundsMin; }
    packed_float3 boundsMax() const { return _boundsMax; }

private:
    const char* materialTypeToString(uint32_t);
    CIESpectrum loadSampledSpectrum(const std::string& material, const std::string& ext, const std::string& base);

private:
    id<MTLBuffer> _indexBuffer = nil;
    id<MTLBuffer> _vertexBuffer = nil;
    id<MTLBuffer> _materialBuffer = nil;
    id<MTLBuffer> _triangleBuffer = nil;
    id<MTLBuffer> _emitterTriangleBuffer = nil;
    Environment _environment;
    packed_float3 _boundsMin;
    packed_float3 _boundsMax;
    uint32_t _triangleCount = 0;
    uint32_t _emitterTriangleCount = 0;
};
