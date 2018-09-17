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

void GeometryProvider::loadFile(const std::string& fileName, id<MTLDevice> device)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string error;
    if (tinyobj::LoadObj(&attrib, &shapes, &materials, &error, fileName.c_str()) == false)
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

    NSLog(@".obj file loaded: %zu vertices, %zu normals, %zu tex coords", attrib.vertices.size(),
          attrib.normals.size(), attrib.texcoords.size());

    uint32_t index = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (const tinyobj::shape_t& shape : shapes)
    {
        indices.reserve(indices.size() + shape.mesh.indices.size());

        for (tinyobj::index_t i : shape.mesh.indices)
        {
            vertices.emplace_back();
            Vertex& vertex = vertices.back();
            vertex.v.x = attrib.vertices[3 * i.vertex_index + 0];
            vertex.v.y = attrib.vertices[3 * i.vertex_index + 1];
            vertex.v.z = attrib.vertices[3 * i.vertex_index + 2];
            vertex.n.x = attrib.normals[3 * i.normal_index + 0];
            vertex.n.y = attrib.normals[3 * i.normal_index + 1];
            vertex.n.z = attrib.normals[3 * i.normal_index + 2];
            vertex.t.x = attrib.texcoords[2 * i.texcoord_index + 0];
            vertex.t.y = attrib.texcoords[2 * i.texcoord_index + 1];
            indices.emplace_back(index++);
        }
    }

    _vertexBuffer = [device newBufferWithBytes:vertices.data() length:sizeof(Vertex) * vertices.size()
        options:MTLResourceStorageModeManaged];
    [_vertexBuffer setLabel:@"Geometry vertex buffer"];

    _indexBuffer = [device newBufferWithBytes:indices.data() length:sizeof(uint32_t) * indices.size()
        options:MTLResourceStorageModeManaged];
    [_indexBuffer setLabel:@"Geometry index buffer"];

    _triangleCount = static_cast<uint32_t>(indices.size() / 3);
}

