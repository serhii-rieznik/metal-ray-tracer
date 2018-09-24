//
//  GeometryProvider.mm
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/17/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <vector>
#include "GeometryProvider.h"
#include "Shaders/structures.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"

class BufferConstructor
{
public:
    BufferConstructor(id<MTLDevice> device) :
        _device(device) { }

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

void GeometryProvider::loadFile(const std::string& fileName, id<MTLDevice> device)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string error;
    std::string baseDir = fileName;
    size_t slash = baseDir.find_last_of('/');
    if (slash != std::string::npos)
        baseDir.erase(slash);

    if (tinyobj::LoadObj(&attrib, &shapes, &materials, &error, fileName.c_str(), baseDir.c_str()) == false)
    {
        NSLog(@"Failed to load .obj file `%s`", fileName.c_str());
        NSLog(@"Error: %s", error.c_str());

        Vertex vertices[3] = {
            { { 0.25f, 0.25f, 0.0f }, {}, {} },
            { { 0.75f, 0.25f, 0.0f }, {}, {} },
            { { 0.50f, 0.75f, 0.0f }, {}, {} }
        };
        uint32_t indices[3] = { 0, 1, 2 };

        _vertexBuffer = [device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeManaged];
        _indexBuffer = [device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeManaged];
        return;
    }

    std::vector<Material> materialBuffer;
    for (const tinyobj::material_t& mtl : materials)
    {
        materialBuffer.emplace_back();
        Material& material = materialBuffer.back();
        material.diffuse.x = mtl.diffuse[0];
        material.diffuse.y = mtl.diffuse[1];
        material.diffuse.z = mtl.diffuse[2];
        material.emissive.x = mtl.emission[0];
        material.emissive.y = mtl.emission[1];
        material.emissive.z = mtl.emission[2];
    }

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;
    std::vector<Triangle> triangleBuffer;
    std::vector<EmitterTriangle> emitterTriangleBuffer;

    uint32_t globalTriangleIndex = 0;
    float totalLightArea = 0.0f;
    for (const tinyobj::shape_t& shape : shapes)
    {
        size_t triangleCount = shape.mesh.num_face_vertices.size();
        _triangleCount += static_cast<uint32_t>(triangleCount);

        size_t triangleBufferOffset = triangleBuffer.size();

        indexBuffer.resize(indexBuffer.size() + triangleCount * 3);
        triangleBuffer.resize(triangleBuffer.size() + triangleCount);

        size_t index = 0;
        for (size_t f = 0; f < triangleCount; ++f)
        {
            assert(shape.mesh.num_face_vertices[f] == 3);
            for (size_t j = 0; j < 3; ++j)
            {
                vertexBuffer.emplace_back();
                Vertex& vertex = vertexBuffer.back();
                const tinyobj::index_t& i = shape.mesh.indices[index];
                vertex.v.x = attrib.vertices[3 * i.vertex_index + 0];
                vertex.v.y = attrib.vertices[3 * i.vertex_index + 1];
                vertex.v.z = attrib.vertices[3 * i.vertex_index + 2];
                vertex.n.x = attrib.normals[3 * i.normal_index + 0];
                vertex.n.y = attrib.normals[3 * i.normal_index + 1];
                vertex.n.z = attrib.normals[3 * i.normal_index + 2];
                vertex.t.x = attrib.texcoords[2 * i.texcoord_index + 0];
                vertex.t.y = attrib.texcoords[2 * i.texcoord_index + 1];
                ++index;
            }

            const Vertex& v0 = vertexBuffer[vertexBuffer.size() - 3];
            const Vertex& v1 = vertexBuffer[vertexBuffer.size() - 2];
            const Vertex& v2 = vertexBuffer[vertexBuffer.size() - 1];
            float area = 0.5f * simd_length(simd_cross(v2.v - v0.v, v1.v - v0.v));

            triangleBuffer[triangleBufferOffset].materialIndex = shape.mesh.material_ids[f];

            const Material& material = materialBuffer[triangleBuffer[triangleBufferOffset].materialIndex];
            if (simd_length(material.emissive) > 0.0f)
            {
                emitterTriangleBuffer.emplace_back();
                emitterTriangleBuffer.back().area = area;
                emitterTriangleBuffer.back().globalIndex = globalTriangleIndex;
                emitterTriangleBuffer.back().v0 = v0;
                emitterTriangleBuffer.back().v1 = v1;
                emitterTriangleBuffer.back().v2 = v2;
                totalLightArea += area;

            }
            ++triangleBufferOffset;
            ++globalTriangleIndex;
        }
    }

    std::stable_sort(std::begin(emitterTriangleBuffer), std::end(emitterTriangleBuffer), [](const EmitterTriangle& l, const EmitterTriangle& r){
        return l.area < r.area;
    });

    _emitterTriangleCount = static_cast<uint32_t>(emitterTriangleBuffer.size());

    float cdf = 0.0f;
    for (EmitterTriangle& t : emitterTriangleBuffer)
    {
        t.cdf = cdf;
        t.pdf = t.area / totalLightArea;
        cdf += t.pdf;
    }
    emitterTriangleBuffer.emplace_back();
    emitterTriangleBuffer.back().cdf = 1.0f;

    for (uint32_t i = 0; i < indexBuffer.size(); ++i)
        indexBuffer[i] = i;

    BufferConstructor makeBuffer(device);
    _vertexBuffer = makeBuffer(vertexBuffer, @"Geometry vertex buffer");
    _indexBuffer = makeBuffer(indexBuffer, @"Geometry index buffer");
    _materialBuffer = makeBuffer(materialBuffer, @"Geometry material buffer");
    _triangleBuffer = makeBuffer(triangleBuffer, @"Geometry triangle buffer");
    _emitterTriangleBuffer = makeBuffer(emitterTriangleBuffer, @"Emitter triangle buffer");

    NSLog(@".obj file loaded: %zu vertices, %zu normals, %zu tex coords, %u triangles, %zu materials", attrib.vertices.size(),
          attrib.normals.size(), attrib.texcoords.size(), _triangleCount, materials.size());
}

