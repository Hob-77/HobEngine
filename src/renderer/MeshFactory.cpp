#include "renderer/MeshFactory.h"
#include "core/Logger.h"
#include <cmath>
#include <limits>

namespace Engine
{
    // Mathematical constants
    constexpr float MeshFactory::PI;
    constexpr float MeshFactory::TWO_PI;
    constexpr float MeshFactory::HALF_PI;

    // Static member definition
    IRenderDevice* MeshFactory::s_renderDevice = nullptr;

    // REQUIRED: Initialize with render device before creating meshes
    void MeshFactory::initialize(IRenderDevice* renderDevice)
    {
        // Guard against double initialization
        if (s_renderDevice != nullptr)
        {
            LOG_WARN("MeshFactory already initialized - ignoring duplicate call");
            return;
        }

        s_renderDevice = renderDevice;
        LOG_INFO("MeshFactory initialized with render device");
    }

    // Helper functions
    std::vector<vec2> MeshFactory::generateCircleVertices(int segments)
    {
        std::vector<vec2> circle;
        circle.reserve(segments + 1);

        const float angleStep = TWO_PI / segments;

        for (int i = 0; i <= segments; ++i)
        {
            float angle = angleStep * i;
            circle.push_back(vec2(std::cos(angle), std::sin(angle)));
        }

        return circle;
    }

    void MeshFactory::calculateSmoothNormals(
        std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        // Zero out all normals (prepare for accumulation)
        for (auto& vertex : vertices)
        {
            vertex.normal = vec3(0.0f);
        }

        // Accumulate face normals at each vertex
        // Face normal magnitude = 2 x triangle area (area-weighted averaging)
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t idx0 = indices[i + 0];
            uint32_t idx1 = indices[i + 1];
            uint32_t idx2 = indices[i + 2];

            const vec3& v0 = vertices[idx0].position;
            const vec3& v1 = vertices[idx1].position;
            const vec3& v2 = vertices[idx2].position;

            // Calculate face normal using cross product
            vec3 edge1 = v1 - v0;
            vec3 edge2 = v2 - v0;
            vec3 faceNormal = cross(edge1, edge2);
            // Don't normalize! Magnitude encodes triangle area (larger faces contribute more)

            // Accumulate this face normal to all 3 vertices
            vertices[idx0].normal += faceNormal;
            vertices[idx1].normal += faceNormal;
            vertices[idx2].normal += faceNormal;
        }

        // Normalize all accumulated normals
        for (auto& vertex : vertices)
        {
            float length = glm::length(vertex.normal);
            if (length > 0.0001f)
            {
                vertex.normal = vertex.normal / length;
            }
            else
            {
                // Degenerate case: default to up vector
                vertex.normal = vec3(0, 1, 0);
                LOG_WARN("Degenerate normal found during smooth normal calculation");
            }
        }
    }

    std::vector<float> MeshFactory::verticesToFloats(const std::vector<Vertex>& vertices)
    {
        std::vector<float> result;
        result.reserve(vertices.size() * 8); // 3 pos + 3 normal + 2 uv

        for (const auto& v : vertices)
        {
            // Position
            result.push_back(v.position.x);
            result.push_back(v.position.y);
            result.push_back(v.position.z);
            // Normal
            result.push_back(v.normal.x);
            result.push_back(v.normal.y);
            result.push_back(v.normal.z);
            // UV
            result.push_back(v.uv.x);
            result.push_back(v.uv.y);
        }

        return result;
    }

    void MeshFactory::calculateBounds(IMesh& mesh, const std::vector<Vertex>& vertices)
    {
        if (vertices.empty())
        {
            LOG_ERROR("Cannot calculate bounds for empty mesh");
            mesh.aabb = { vec3(0), vec3(0) };
            mesh.boundingSphere = { vec3(0), 0.0f };
            return;
        }

        // Calculate AABB
        vec3 min(std::numeric_limits<float>::max());
        vec3 max(std::numeric_limits<float>::lowest());

        for (const auto& v : vertices)
        {
            min.x = std::min(min.x, v.position.x);
            min.y = std::min(min.y, v.position.y);
            min.z = std::min(min.z, v.position.z);

            max.x = std::max(max.x, v.position.x);
            max.y = std::max(max.y, v.position.y);
            max.z = std::max(max.z, v.position.z);
        }

        mesh.aabb = { min, max };

        // Calculate bounding sphere (center at AABB center, radius to farthest vertex)
        vec3 center = (min + max) * 0.5f;
        float maxRadiusSq = 0.0f;

        for (const auto& v : vertices)
        {
            vec3 offset = v.position - center;
            float distSq = dot(offset, offset);
            maxRadiusSq = std::max(maxRadiusSq, distSq);
        }

        // Compute base radius
        float radius = std::sqrt(maxRadiusSq);

        // Add 5% padding to avoid clipping due to precision errors
        radius *= 1.05f;

        mesh.boundingSphere = { center, radius };
    }

    // === BASIC PRIMITIVES ===

    std::shared_ptr<IMesh> MeshFactory::createCube(float size)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        float half = size * 0.5f;

        // 24 vertices (4 per face, no sharing due to different normals/UVs per face)
        std::vector<Vertex> vertices = {
            // Front face (Z+)
            {{-half, -half,  half}, {0, 0, 1}, {0, 0}},
            {{ half, -half,  half}, {0, 0, 1}, {1, 0}},
            {{ half,  half,  half}, {0, 0, 1}, {1, 1}},
            {{-half,  half,  half}, {0, 0, 1}, {0, 1}},

            // Back face (Z-)
            {{ half, -half, -half}, {0, 0, -1}, {0, 0}},
            {{-half, -half, -half}, {0, 0, -1}, {1, 0}},
            {{-half,  half, -half}, {0, 0, -1}, {1, 1}},
            {{ half,  half, -half}, {0, 0, -1}, {0, 1}},

            // Left face (X-)
            {{-half, -half, -half}, {-1, 0, 0}, {0, 0}},
            {{-half, -half,  half}, {-1, 0, 0}, {1, 0}},
            {{-half,  half,  half}, {-1, 0, 0}, {1, 1}},
            {{-half,  half, -half}, {-1, 0, 0}, {0, 1}},

            // Right face (X+)
            {{ half, -half,  half}, { 1, 0, 0}, {0, 0}},
            {{ half, -half, -half}, { 1, 0, 0}, {1, 0}},
            {{ half,  half, -half}, { 1, 0, 0}, {1, 1}},
            {{ half,  half,  half}, { 1, 0, 0}, {0, 1}},

            // Bottom face (Y-)
            {{-half, -half, -half}, {0, -1, 0}, {0, 0}},
            {{ half, -half, -half}, {0, -1, 0}, {1, 0}},
            {{ half, -half,  half}, {0, -1, 0}, {1, 1}},
            {{-half, -half,  half}, {0, -1, 0}, {0, 1}},

            // Top face (Y+)
            {{-half,  half,  half}, {0,  1, 0}, {0, 0}},
            {{ half,  half,  half}, {0,  1, 0}, {1, 0}},
            {{ half,  half, -half}, {0,  1, 0}, {1, 1}},
            {{-half,  half, -half}, {0,  1, 0}, {0, 1}}
        };

        // 36 indices (6 faces x 2 triangles x 3 vertices)
        std::vector<uint32_t> indices = {
            0, 1, 2,    2, 3, 0,     // Front
            4, 5, 6,    6, 7, 4,     // Back
            8, 9, 10,   10, 11, 8,   // Left
            12, 13, 14, 14, 15, 12,  // Right
            16, 17, 18, 18, 19, 16,  // Bottom
            20, 21, 22, 22, 23, 20   // Top
        };

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created cube (size: {}, vertices: {}, indices: {})",
            size, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createSphere(float radius, int sectors, int stacks)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float sectorStep = TWO_PI / sectors;
        float stackStep = PI / stacks;

        // Generate vertices (latitude/longitude grid)
        for (int i = 0; i <= stacks; ++i)
        {
            float stackAngle = HALF_PI - i * stackStep;
            float xy = radius * std::cos(stackAngle);
            float z = radius * std::sin(stackAngle);

            for (int j = 0; j <= sectors; ++j)
            {
                float sectorAngle = j * sectorStep;

                Vertex vertex;
                vertex.position.x = xy * std::cos(sectorAngle);
                vertex.position.y = z;
                vertex.position.z = xy * std::sin(sectorAngle);
                vertex.normal = normalize(vertex.position);
                vertex.uv.x = (float)j / sectors;
                vertex.uv.y = (float)i / stacks;

                vertices.push_back(vertex);
            }
        }

        // Generate indices (reversed winding for correct culling)
        for (int i = 0; i < stacks; ++i)
        {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;

            for (int j = 0; j < sectors; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                }

                if (i != (stacks - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2 + 1);
                    indices.push_back(k2);
                }
            }
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created sphere (radius: {}, sectors: {}, stacks: {}, vertices: {}, indices: {})",
            radius, sectors, stacks, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createIcosphere(float radius, int subdivisions)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Generate base icosahedron using golden ratio
        const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;  // ~ 1.618
        const float a = 1.0f;
        const float b = 1.0f / phi;  // ~ 0.618

        // 12 vertices arranged in 3 orthogonal golden rectangles
        std::vector<vec3> positions = {
            {-b,  a,  0}, { b,  a,  0}, {-b, -a,  0}, { b, -a,  0},  // XY plane
            { 0, -b,  a}, { 0,  b,  a}, { 0, -b, -a}, { 0,  b, -a},  // YZ plane
            { a,  0, -b}, { a,  0,  b}, {-a,  0, -b}, {-a,  0,  b}   // XZ plane
        };

        // Normalize to unit sphere, then scale to radius
        for (auto& pos : positions) {
            pos = normalize(pos) * radius;
        }

        // Define 20 triangular faces of icosahedron
        struct Triangle {
            uint32_t v0, v1, v2;
        };

        std::vector<Triangle> faces = {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},  // Around vertex 0
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  // Adjacent
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},      // Around vertex 3
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}     // Adjacent
        };

        // Subdivide faces (each subdivision creates 4 triangles from 1)
        for (int sub = 0; sub < subdivisions; ++sub)
        {
            std::vector<Triangle> newFaces;
            std::unordered_map<uint64_t, uint32_t> midpointCache;

            auto getMidpoint = [&](uint32_t v1, uint32_t v2) -> uint32_t
                {
                    // Create unique edge key (order-independent)
                    uint64_t key = (v1 < v2) ?
                        ((uint64_t)v1 << 32) | v2 :
                        ((uint64_t)v2 << 32) | v1;

                    auto it = midpointCache.find(key);
                    if (it != midpointCache.end()) {
                        return it->second;
                    }

                    // Create and project midpoint onto sphere
                    vec3 mid = normalize((positions[v1] + positions[v2]) * 0.5f) * radius;

                    uint32_t index = static_cast<uint32_t>(positions.size());
                    positions.push_back(mid);
                    midpointCache[key] = index;

                    return index;
                };

            // Subdivide each triangle into 4
            for (const auto& face : faces)
            {
                uint32_t a = getMidpoint(face.v0, face.v1);
                uint32_t b = getMidpoint(face.v1, face.v2);
                uint32_t c = getMidpoint(face.v2, face.v0);

                newFaces.push_back({ face.v0, a, c });  // Corner 1
                newFaces.push_back({ face.v1, b, a });  // Corner 2
                newFaces.push_back({ face.v2, c, b });  // Corner 3
                newFaces.push_back({ a, b, c });        // Center
            }

            faces = newFaces;
        }

        // Create vertex data with normals and UVs
        for (const auto& pos : positions)
        {
            Vertex v;
            v.position = pos;
            v.normal = normalize(pos);

            // Spherical UV mapping (no pole distortion, but still has seam)
            float u = 0.5f + std::atan2(pos.z, pos.x) / TWO_PI;
            float v_coord = 0.5f - std::asin(pos.y / radius) / PI;
            v.uv = vec2(u, v_coord);

            vertices.push_back(v);
        }

        // Create indices (natural CCW winding from icosahedron)
        for (const auto& face : faces)
        {
            indices.push_back(face.v0);
            indices.push_back(face.v1);
            indices.push_back(face.v2);
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created icosphere (radius: {}, subdivisions: {}, vertices: {}, indices: {})",
            radius, subdivisions, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createPlane(float width, float depth, int segmentsX, int segmentsZ)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfWidth = width * 0.5f;
        float halfDepth = depth * 0.5f;

        // Top face vertices (normal +Y)
        int topStartIndex = 0;
        for (int z = 0; z <= segmentsZ; ++z)
        {
            for (int x = 0; x <= segmentsX; ++x)
            {
                float xPos = -halfWidth + (width / segmentsX) * x;
                float zPos = -halfDepth + (depth / segmentsZ) * z;

                Vertex vertex;
                vertex.position = vec3(xPos, 0.0f, zPos);
                vertex.normal = vec3(0.0f, 1.0f, 0.0f);
                vertex.uv = vec2((float)x / segmentsX, (float)z / segmentsZ);

                vertices.push_back(vertex);
            }
        }

        // Bottom face vertices (normal -Y, same positions)
        int bottomStartIndex = static_cast<int>(vertices.size());
        for (int z = 0; z <= segmentsZ; ++z)
        {
            for (int x = 0; x <= segmentsX; ++x)
            {
                float xPos = -halfWidth + (width / segmentsX) * x;
                float zPos = -halfDepth + (depth / segmentsZ) * z;

                Vertex vertex;
                vertex.position = vec3(xPos, 0.0f, zPos);
                vertex.normal = vec3(0.0f, -1.0f, 0.0f);
                vertex.uv = vec2((float)x / segmentsX, (float)z / segmentsZ);

                vertices.push_back(vertex);
            }
        }

        // Top face indices (CCW from above)
        for (int z = 0; z < segmentsZ; ++z)
        {
            for (int x = 0; x < segmentsX; ++x)
            {
                int topLeft = topStartIndex + z * (segmentsX + 1) + x;
                int topRight = topLeft + 1;
                int bottomLeft = topStartIndex + (z + 1) * (segmentsX + 1) + x;
                int bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        // Bottom face indices (CCW from below)
        for (int z = 0; z < segmentsZ; ++z)
        {
            for (int x = 0; x < segmentsX; ++x)
            {
                int topLeft = bottomStartIndex + z * (segmentsX + 1) + x;
                int topRight = topLeft + 1;
                int bottomLeft = bottomStartIndex + (z + 1) * (segmentsX + 1) + x;
                int bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(topRight);
                indices.push_back(bottomLeft);

                indices.push_back(topRight);
                indices.push_back(bottomRight);
                indices.push_back(bottomLeft);
            }
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created plane ({}x{}, segments: {}x{}, vertices: {}, indices: {})",
            width, depth, segmentsX, segmentsZ, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createCylinder(float radius, float height, int segments)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfHeight = height * 0.5f;
        auto circle = generateCircleVertices(segments);

        // Side vertices (bottom and top rings)
        for (int ring = 0; ring < 2; ++ring)
        {
            float y = (ring == 0) ? -halfHeight : halfHeight;
            float v = (ring == 0) ? 0.0f : 1.0f;

            for (int i = 0; i <= segments; ++i)
            {
                float x = circle[i].x * radius;
                float z = circle[i].y * radius;

                Vertex vert;
                vert.position = vec3(x, y, z);
                vert.normal = normalize(vec3(circle[i].x, 0, circle[i].y));
                vert.uv = vec2((float)i / segments, v);
                vertices.push_back(vert);
            }
        }

        // Bottom cap (center + ring)
        uint32_t bottomCapStart = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ {0, -halfHeight, 0}, {0, -1, 0}, {0.5f, 0.5f} });

        for (int i = 0; i <= segments; ++i)
        {
            float x = circle[i].x * radius;
            float z = circle[i].y * radius;

            Vertex v;
            v.position = vec3(x, -halfHeight, z);
            v.normal = vec3(0, -1, 0);
            v.uv = vec2(0.5f + 0.5f * circle[i].x, 0.5f + 0.5f * circle[i].y);
            vertices.push_back(v);
        }

        // Top cap (center + ring)
        uint32_t topCapStart = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ {0, halfHeight, 0}, {0, 1, 0}, {0.5f, 0.5f} });

        for (int i = 0; i <= segments; ++i)
        {
            float x = circle[i].x * radius;
            float z = circle[i].y * radius;

            Vertex v;
            v.position = vec3(x, halfHeight, z);
            v.normal = vec3(0, 1, 0);
            v.uv = vec2(0.5f + 0.5f * circle[i].x, 0.5f + 0.5f * circle[i].y);
            vertices.push_back(v);
        }

        // Side face indices (reversed winding)
        for (int i = 0; i < segments; ++i)
        {
            int bottomCurrent = i;
            int bottomNext = i + 1;
            int topCurrent = (segments + 1) + i;
            int topNext = (segments + 1) + i + 1;

            indices.push_back(bottomCurrent);
            indices.push_back(topNext);
            indices.push_back(bottomNext);

            indices.push_back(bottomCurrent);
            indices.push_back(topCurrent);
            indices.push_back(topNext);
        }

        // Bottom cap indices (reversed winding)
        for (int i = 0; i < segments; ++i)
        {
            int currentEdge = i;
            int nextEdge = (i + 1) % segments;

            indices.push_back(bottomCapStart);
            indices.push_back(bottomCapStart + 1 + currentEdge);
            indices.push_back(bottomCapStart + 1 + nextEdge);
        }

        // Top cap indices (reversed winding)
        for (int i = 0; i < segments; ++i)
        {
            int currentEdge = i;
            int nextEdge = (i + 1) % segments;

            indices.push_back(topCapStart);
            indices.push_back(topCapStart + 1 + nextEdge);
            indices.push_back(topCapStart + 1 + currentEdge);
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created cylinder (radius: {}, height: {}, segments: {}, vertices: {}, indices: {})",
            radius, height, segments, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createQuad(float width, float height)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        float halfW = width * 0.5f;
        float halfH = height * 0.5f;

        // Double-sided quad (front + back faces)
        std::vector<Vertex> vertices = {
            // Front face (normal +Z)
            {{-halfW, -halfH, 0}, {0, 0, 1}, {0, 0}},
            {{ halfW, -halfH, 0}, {0, 0, 1}, {1, 0}},
            {{ halfW,  halfH, 0}, {0, 0, 1}, {1, 1}},
            {{-halfW,  halfH, 0}, {0, 0, 1}, {0, 1}},

            // Back face (normal -Z)
            {{-halfW, -halfH, 0}, {0, 0, -1}, {0, 0}},
            {{ halfW, -halfH, 0}, {0, 0, -1}, {1, 0}},
            {{ halfW,  halfH, 0}, {0, 0, -1}, {1, 1}},
            {{-halfW,  halfH, 0}, {0, 0, -1}, {0, 1}}
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,  2, 3, 0,  // Front (CCW from front)
            5, 4, 6,  6, 4, 7   // Back (CCW from back)
        };

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created quad ({}x{}, vertices: {}, indices: {})",
            width, height, vertices.size(), indices.size());

        return mesh;
    }

    // === ADVANCED PRIMITIVES ===

    std::shared_ptr<IMesh> MeshFactory::createCone(float radius, float height, int segments)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto circle = generateCircleVertices(segments);

        // Apex vertex
        vertices.push_back({ {0, height, 0}, {0, 1, 0}, {0.5f, 0.5f} });
        uint32_t apexIndex = 0;

        // Side vertices (base ring with smooth normals)
        for (int i = 0; i <= segments; ++i)
        {
            float x = circle[i].x * radius;
            float z = circle[i].y * radius;

            vec3 toApex = normalize(vec3(0, height, 0) - vec3(x, 0, z));
            vec3 outward = normalize(vec3(circle[i].x, 0, circle[i].y));
            vec3 normal = normalize(toApex + outward);

            Vertex v;
            v.position = vec3(x, 0, z);
            v.normal = normal;
            v.uv = vec2((float)i / segments, 1.0f);
            vertices.push_back(v);
        }

        // Base cap (center + ring)
        uint32_t baseCapStart = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ {0, 0, 0}, {0, -1, 0}, {0.5f, 0.5f} });

        for (int i = 0; i <= segments; ++i)
        {
            float x = circle[i].x * radius;
            float z = circle[i].y * radius;

            Vertex v;
            v.position = vec3(x, 0, z);
            v.normal = vec3(0, -1, 0);
            v.uv = vec2(0.5f + 0.5f * circle[i].x, 0.5f + 0.5f * circle[i].y);
            vertices.push_back(v);
        }

        // Side triangle indices
        for (int i = 0; i < segments; ++i)
        {
            uint32_t current = 1 + i;
            uint32_t next = 1 + i + 1;

            indices.push_back(apexIndex);
            indices.push_back(next);
            indices.push_back(current);
        }

        // Base cap indices (reversed winding)
        for (int i = 0; i < segments; ++i)
        {
            int currentEdge = i;
            int nextEdge = (i + 1) % segments;

            indices.push_back(baseCapStart);
            indices.push_back(baseCapStart + 1 + currentEdge);
            indices.push_back(baseCapStart + 1 + nextEdge);
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created cone (radius: {}, height: {}, segments: {}, vertices: {}, indices: {})",
            radius, height, segments, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createPyramid(float baseSize, float height)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        float half = baseSize * 0.5f;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vec3 apex(0, height, 0);
        vec3 bl(-half, 0, -half);  // Bottom-left
        vec3 br(half, 0, -half);   // Bottom-right
        vec3 tr(half, 0, half);    // Top-right
        vec3 tl(-half, 0, half);   // Top-left

        auto calcNormal = [](const vec3& v0, const vec3& v1, const vec3& v2) {
            return normalize(cross(v1 - v0, v2 - v0));
            };

        // Front face
        vec3 frontNormal = calcNormal(apex, bl, br);
        vertices.push_back({ apex, frontNormal, {0.5f, 1.0f} });
        vertices.push_back({ bl,   frontNormal, {0.0f, 0.0f} });
        vertices.push_back({ br,   frontNormal, {1.0f, 0.0f} });

        // Right face
        vec3 rightNormal = calcNormal(apex, br, tr);
        vertices.push_back({ apex, rightNormal, {0.5f, 1.0f} });
        vertices.push_back({ br,   rightNormal, {0.0f, 0.0f} });
        vertices.push_back({ tr,   rightNormal, {1.0f, 0.0f} });

        // Back face
        vec3 backNormal = calcNormal(apex, tr, tl);
        vertices.push_back({ apex, backNormal, {0.5f, 1.0f} });
        vertices.push_back({ tr,   backNormal, {0.0f, 0.0f} });
        vertices.push_back({ tl,   backNormal, {1.0f, 0.0f} });

        // Left face
        vec3 leftNormal = calcNormal(apex, tl, bl);
        vertices.push_back({ apex, leftNormal, {0.5f, 1.0f} });
        vertices.push_back({ tl,   leftNormal, {0.0f, 0.0f} });
        vertices.push_back({ bl,   leftNormal, {1.0f, 0.0f} });

        // Base (square)
        vec3 baseNormal(0, -1, 0);
        vertices.push_back({ bl, baseNormal, {0.0f, 0.0f} });
        vertices.push_back({ br, baseNormal, {1.0f, 0.0f} });
        vertices.push_back({ tr, baseNormal, {1.0f, 1.0f} });
        vertices.push_back({ tl, baseNormal, {0.0f, 1.0f} });

        // Side face indices (reversed winding)
        indices.push_back(0);  indices.push_back(2);  indices.push_back(1);   // Front
        indices.push_back(3);  indices.push_back(5);  indices.push_back(4);   // Right
        indices.push_back(6);  indices.push_back(8);  indices.push_back(7);   // Back
        indices.push_back(9);  indices.push_back(11); indices.push_back(10);  // Left

        // Base indices (reversed winding)
        indices.push_back(12); indices.push_back(13); indices.push_back(14);
        indices.push_back(12); indices.push_back(14); indices.push_back(15);

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created pyramid (base: {}, height: {}, vertices: {}, indices: {})",
            baseSize, height, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createCapsule(float radius, float height, int segments, int rings)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto circle = generateCircleVertices(segments);

        float cylinderHeight = height - 2.0f * radius;
        if (cylinderHeight < 0.0f)
        {
            LOG_WARN("Capsule height ({}) too small for radius ({}), clamping cylinder height to 0",
                height, radius);
            cylinderHeight = 0.0f;
        }

        float halfCylinderHeight = cylinderHeight * 0.5f;

        // Top hemisphere (north pole to equator)
        for (int stack = 0; stack <= rings; ++stack)
        {
            float stackAngle = HALF_PI - (HALF_PI / rings) * stack;
            float xy = radius * std::cos(stackAngle);
            float y = radius * std::sin(stackAngle) + halfCylinderHeight;

            for (int i = 0; i <= segments; ++i)
            {
                float x = xy * circle[i].x;
                float z = xy * circle[i].y;

                Vertex v;
                v.position = vec3(x, y, z);
                v.normal = normalize(vec3(x, y - halfCylinderHeight, z));
                v.uv = vec2((float)i / segments, (float)stack / (rings * 2));
                vertices.push_back(v);
            }
        }

        // Bottom hemisphere (equator to south pole)
        for (int stack = 0; stack <= rings; ++stack)
        {
            float stackAngle = -(HALF_PI / rings) * stack;
            float xy = radius * std::cos(stackAngle);
            float y = radius * std::sin(stackAngle) - halfCylinderHeight;

            for (int i = 0; i <= segments; ++i)
            {
                float x = xy * circle[i].x;
                float z = xy * circle[i].y;

                Vertex v;
                v.position = vec3(x, y, z);
                v.normal = normalize(vec3(x, y + halfCylinderHeight, z));
                v.uv = vec2((float)i / segments, 0.5f + (float)stack / (rings * 2));
                vertices.push_back(v);
            }
        }

        // Generate indices with corrected stack count
        int totalRings = (rings + 1) * 2;
        int totalStacks = totalRings - 1;

        for (int stack = 0; stack < totalStacks; ++stack)
        {
            int k1 = stack * (segments + 1);
            int k2 = k1 + segments + 1;

            for (int i = 0; i < segments; ++i, ++k1, ++k2)
            {
                indices.push_back(k1);
                indices.push_back(k1 + 1);
                indices.push_back(k2);

                indices.push_back(k1 + 1);
                indices.push_back(k2 + 1);
                indices.push_back(k2);
            }
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created capsule (radius: {}, height: {}, segments: {}, rings: {}, vertices: {}, indices: {})",
            radius, height, segments, rings, vertices.size(), indices.size());

        return mesh;
    }

    std::shared_ptr<IMesh> MeshFactory::createTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto majorCircle = generateCircleVertices(majorSegments);
        auto minorCircle = generateCircleVertices(minorSegments);

        // Sweep minor circle around major circle
        for (int i = 0; i <= majorSegments; ++i)
        {
            vec2 majorPos = majorCircle[i];
            vec3 majorCenter(majorPos.x * majorRadius, 0, majorPos.y * majorRadius);

            for (int j = 0; j <= minorSegments; ++j)
            {
                vec2 minorPos = minorCircle[j];

                vec3 normal = normalize(vec3(
                    majorPos.x * minorPos.x,
                    minorPos.y,
                    majorPos.y * minorPos.x
                ));

                vec3 position = majorCenter + normal * minorRadius;

                Vertex v;
                v.position = position;
                v.normal = normal;
                v.uv = vec2((float)i / majorSegments, (float)j / minorSegments);
                vertices.push_back(v);
            }
        }

        // Stitch tube together
        for (int i = 0; i < majorSegments; ++i)
        {
            for (int j = 0; j < minorSegments; ++j)
            {
                int current = i * (minorSegments + 1) + j;
                int next = current + minorSegments + 1;

                indices.push_back(current);
                indices.push_back(current + 1);
                indices.push_back(next);

                indices.push_back(current + 1);
                indices.push_back(next + 1);
                indices.push_back(next);
            }
        }

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created torus (majorR: {}, minorR: {}, majorSegs: {}, minorSegs: {}, vertices: {}, indices: {})",
            majorRadius, minorRadius, majorSegments, minorSegments, vertices.size(), indices.size());

        return mesh;
    }

    // === SPECIAL PRIMITIVES ===

    std::shared_ptr<IMesh> MeshFactory::createSkyboxCube(float size)
    {
        if (!s_renderDevice)
        {
            LOG_ERROR("MeshFactory not initialized! Call initialize(renderDevice) first.");
            return nullptr;
        }

        float half = size * 0.5f;

        // Inverted normals (point inward) and reversed winding (CW from outside)
        // Allows rendering from inside with standard back-face culling
        std::vector<Vertex> vertices = {
            // Front face (Z+)
            {{ half, -half,  half}, {0, 0, -1}, {0, 0}},
            {{-half, -half,  half}, {0, 0, -1}, {1, 0}},
            {{-half,  half,  half}, {0, 0, -1}, {1, 1}},
            {{ half,  half,  half}, {0, 0, -1}, {0, 1}},

            // Back face (Z-)
            {{-half, -half, -half}, {0, 0, 1}, {0, 0}},
            {{ half, -half, -half}, {0, 0, 1}, {1, 0}},
            {{ half,  half, -half}, {0, 0, 1}, {1, 1}},
            {{-half,  half, -half}, {0, 0, 1}, {0, 1}},

            // Left face (X-)
            {{-half, -half,  half}, {1, 0, 0}, {0, 0}},
            {{-half, -half, -half}, {1, 0, 0}, {1, 0}},
            {{-half,  half, -half}, {1, 0, 0}, {1, 1}},
            {{-half,  half,  half}, {1, 0, 0}, {0, 1}},

            // Right face (X+)
            {{ half, -half, -half}, {-1, 0, 0}, {0, 0}},
            {{ half, -half,  half}, {-1, 0, 0}, {1, 0}},
            {{ half,  half,  half}, {-1, 0, 0}, {1, 1}},
            {{ half,  half, -half}, {-1, 0, 0}, {0, 1}},

            // Bottom face (Y-)
            {{ half, -half, -half}, {0, 1, 0}, {0, 0}},
            {{-half, -half, -half}, {0, 1, 0}, {1, 0}},
            {{-half, -half,  half}, {0, 1, 0}, {1, 1}},
            {{ half, -half,  half}, {0, 1, 0}, {0, 1}},

            // Top face (Y+)
            {{ half,  half,  half}, {0, -1, 0}, {0, 0}},
            {{-half,  half,  half}, {0, -1, 0}, {1, 0}},
            {{-half,  half, -half}, {0, -1, 0}, {1, 1}},
            {{ half,  half, -half}, {0, -1, 0}, {0, 1}}
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,    2, 3, 0,     // Front
            4, 5, 6,    6, 7, 4,     // Back
            8, 9, 10,   10, 11, 8,   // Left
            12, 13, 14, 14, 15, 12,  // Right
            16, 17, 18, 18, 19, 16,  // Bottom
            20, 21, 22, 22, 23, 20   // Top
        };

        auto floatData = verticesToFloats(vertices);

        auto mesh = s_renderDevice->createMesh(
            floatData.data(),
            floatData.size(),
            indices.data(),
            indices.size(),
            VertexFormat::PositionNormalUV
        );

        calculateBounds(*mesh, vertices);

        LOG_INFO("Created skybox cube (size: {}, inverted normals/winding for interior viewing)",
            size);

        return mesh;
    }
}