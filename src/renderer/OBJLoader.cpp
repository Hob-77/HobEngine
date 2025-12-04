#include "renderer/OBJLoader.h"
#include "renderer/AssetManager.h"
#include "renderer/MeshFactory.h"
#include "core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cfloat>

namespace Engine
{
    // Helper Functions

    std::string OBJLoader::normalizePath(const std::string& path)
    {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }

    std::string OBJLoader::getDirectory(const std::string& filepath)
    {
        std::string normalized = normalizePath(filepath);
        size_t lastSlash = normalized.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            return normalized.substr(0, lastSlash + 1);
        }
        return "";
    }

    std::string OBJLoader::getBasename(const std::string& filepath)
    {
        std::string normalized = normalizePath(filepath);
        size_t lastSlash = normalized.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            return normalized.substr(lastSlash + 1);
        }
        return normalized;
    }

    // Main Load Function

    std::vector<OBJLoader::ModelData> OBJLoader::load(const std::string& filepath)
    {
        LOG_INFO("Loading OBJ file: {}", filepath);

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open OBJ file: {}", filepath);
            return {};
        }

        // Global vertex data arrays (1-indexed in OBJ!)
        std::vector<vec3> positions;
        std::vector<vec2> texCoords;
        std::vector<vec3> normals;

        // Materials loaded from MTL file
        std::unordered_map<std::string, Material> materials;

        // Current model being built
        std::vector<ModelBuilder> builders;
        ModelBuilder* current = nullptr;

        std::string baseDir = getDirectory(filepath);
        std::string line;
        [[maybe_unused]] int lineNum = 0;

        while (std::getline(file, line))
        {
            lineNum++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v")  // Vertex position
            {
                vec3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vt")  // Texture coordinate
            {
                vec2 tex;
                iss >> tex.x >> tex.y;
                texCoords.push_back(tex);
            }
            else if (prefix == "vn")  // Normal
            {
                vec3 norm;
                iss >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            }
            else if (prefix == "f")  // Face
            {
                // Create default model if none exists
                if (!current)
                {
                    builders.emplace_back();
                    current = &builders.back();
                    current->name = "default";
                }

                parseFace(iss, *current, positions, texCoords, normals);
            }
            else if (prefix == "o" || prefix == "g")  // Object/Group
            {
                builders.emplace_back();
                current = &builders.back();
                iss >> current->name;
                if (current->name.empty())
                {
                    current->name = "object_" + std::to_string(builders.size());
                }
            }
            else if (prefix == "mtllib")  // Material library
            {
                std::string mtlFile;
                iss >> mtlFile;

                std::string mtlPath = baseDir + mtlFile;
                auto loadedMaterials = loadMTL(mtlPath, baseDir);
                materials.insert(loadedMaterials.begin(), loadedMaterials.end());
            }
            else if (prefix == "usemtl")  // Use material
            {
                if (current)
                {
                    // If current builder has geometry, finalize it first
                    if (!current->vertices.empty())
                    {
                        builders.emplace_back();
                        current = &builders.back();
                    }

                    iss >> current->materialName;
                }
            }
        }

        // Convert all builders to final ModelData
        std::vector<ModelData> result;
        for (auto& builder : builders)
        {
            if (!builder.vertices.empty())
            {
                result.push_back(finalize(builder, materials));
            }
        }

        LOG_INFO("Loaded {} object(s) from {}", result.size(), filepath);
        return result;
    }

    // Face Parsing

    OBJLoader::OBJVertex OBJLoader::parseVertexRef(const std::string& token)
    {
        OBJVertex vertex;

        size_t firstSlash = token.find('/');
        if (firstSlash == std::string::npos)
        {
            // Format: v
            vertex.posIndex = std::stoi(token);
        }
        else
        {
            size_t secondSlash = token.find('/', firstSlash + 1);

            // Position
            vertex.posIndex = std::stoi(token.substr(0, firstSlash));

            if (secondSlash == std::string::npos)
            {
                // Format: v/vt
                std::string texStr = token.substr(firstSlash + 1);
                if (!texStr.empty())
                {
                    vertex.texIndex = std::stoi(texStr);
                }
            }
            else
            {
                // Format: v/vt/vn or v//vn
                std::string texStr = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                if (!texStr.empty())
                {
                    vertex.texIndex = std::stoi(texStr);
                }

                std::string normStr = token.substr(secondSlash + 1);
                if (!normStr.empty())
                {
                    vertex.normIndex = std::stoi(normStr);
                }
            }
        }

        return vertex;
    }

    uint32_t OBJLoader::getOrCreateVertex(
        ModelBuilder& builder,
        const OBJVertex& objVertex,
        const std::vector<vec3>& positions,
        const std::vector<vec2>& texCoords,
        const std::vector<vec3>& normals)
    {
        // Check if vertex already exists
        auto it = builder.vertexMap.find(objVertex);
        if (it != builder.vertexMap.end())
        {
            return it->second;
        }

        // Create new vertex
        Vertex vertex;

        // Position (OBJ is 1-indexed, can be negative for relative)
        int posIdx = objVertex.posIndex;
        if (posIdx < 0) posIdx = static_cast<int>(positions.size()) + posIdx + 1;
        if (posIdx > 0 && posIdx <= positions.size())
        {
            vertex.position = positions[posIdx - 1];
        }

        // Texture coordinate
        int texIdx = objVertex.texIndex;
        if (texIdx < 0) texIdx = static_cast<int>(texCoords.size()) + texIdx + 1;
        if (texIdx > 0 && texIdx <= texCoords.size())
        {
            vertex.uv = texCoords[texIdx - 1];
        }
        else
        {
            vertex.uv = vec2(0.0f, 0.0f);
        }

        // Normal
        int normIdx = objVertex.normIndex;
        if (normIdx < 0) normIdx = static_cast<int>(normals.size()) + normIdx + 1;
        if (normIdx > 0 && normIdx <= normals.size())
        {
            vertex.normal = normals[normIdx - 1];
        }
        else
        {
            vertex.normal = vec3(0.0f, 1.0f, 0.0f);  // Default up
        }

        // Add to builder
        uint32_t index = static_cast<uint32_t>(builder.vertices.size());
        builder.vertices.push_back(vertex);
        builder.vertexMap[objVertex] = index;

        return index;
    }

    void OBJLoader::parseFace(
        std::istringstream& iss,
        ModelBuilder& builder,
        const std::vector<vec3>& positions,
        const std::vector<vec2>& texCoords,
        const std::vector<vec3>& normals)
    {
        std::vector<uint32_t> faceIndices;
        std::string token;

        while (iss >> token)
        {
            OBJVertex objVertex = parseVertexRef(token);
            uint32_t index = getOrCreateVertex(builder, objVertex, positions, texCoords, normals);
            faceIndices.push_back(index);
        }

        // Triangulate face (fan triangulation for quads/polygons)
        if (faceIndices.size() >= 3)
        {
            for (size_t i = 1; i < faceIndices.size() - 1; ++i)
            {
                builder.indices.push_back(faceIndices[0]);
                builder.indices.push_back(faceIndices[i]);
                builder.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    // Finalization

    OBJLoader::ModelData OBJLoader::finalize(
        ModelBuilder& builder,
        const std::unordered_map<std::string, Material>& materials)
    {
        ModelData data;
        data.name = builder.name;
        data.materialName = builder.materialName;

        // Look up material
        auto it = materials.find(builder.materialName);
        if (it != materials.end())
        {
            data.material = it->second;
        }
        else if (!builder.materialName.empty())
        {
            LOG_WARN("Material '{}' not found, using default", builder.materialName);
        }

        // Convert vertices to float array
        std::vector<float> vertexData;
        vertexData.reserve(builder.vertices.size() * 8);

        for (const auto& v : builder.vertices)
        {
            vertexData.push_back(v.position.x);
            vertexData.push_back(v.position.y);
            vertexData.push_back(v.position.z);
            vertexData.push_back(v.normal.x);
            vertexData.push_back(v.normal.y);
            vertexData.push_back(v.normal.z);
            vertexData.push_back(v.uv.x);
            vertexData.push_back(v.uv.y);
        }

        // Create mesh via render device (from AssetManager)
        IRenderDevice* renderDevice = AssetManager::get().getRenderDevice();
        if (!renderDevice)
        {
            LOG_ERROR("Cannot create mesh: AssetManager not initialized with render device");
            return data;
        }

        data.mesh = renderDevice->createMesh(
            vertexData.data(),
            vertexData.size(),
            builder.indices.data(),
            builder.indices.size(),
            VertexFormat::PositionNormalUV
        );

        if (!data.mesh)
        {
            LOG_ERROR("Failed to create mesh for object '{}'", data.name);
            return data;
        }

        // Calculate AABB (Axis-Aligned Bounding Box)
        vec3 min(FLT_MAX, FLT_MAX, FLT_MAX);
        vec3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const auto& v : builder.vertices)
        {
            min.x = std::min(min.x, v.position.x);
            min.y = std::min(min.y, v.position.y);
            min.z = std::min(min.z, v.position.z);

            max.x = std::max(max.x, v.position.x);
            max.y = std::max(max.y, v.position.y);
            max.z = std::max(max.z, v.position.z);
        }

        data.mesh->aabb.min = min;
        data.mesh->aabb.max = max;

        // Calculate Bounding Sphere (center + radius)
        vec3 center = (min + max) * 0.5f;
        float radius = 0.0f;

        for (const auto& v : builder.vertices)
        {
            float dist = length(v.position - center);
            radius = std::max(radius, dist);
        }

        data.mesh->boundingSphere.center = center;
        data.mesh->boundingSphere.radius = radius;

        LOG_INFO("  Object '{}': {} vertices, {} indices, material '{}', bounds: [{:.1f}, {:.1f}, {:.1f}] to [{:.1f}, {:.1f}, {:.1f}]",
            data.name, builder.vertices.size(), builder.indices.size(), data.materialName,
            min.x, min.y, min.z, max.x, max.y, max.z);

        return data;
    }

    // MTL Loading

    std::unordered_map<std::string, Material> OBJLoader::loadMTL(
        const std::string& filepath,
        const std::string& baseDir)
    {
        LOG_INFO("Loading MTL file: {}", filepath);

        std::unordered_map<std::string, Material> materials;

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            LOG_WARN("Failed to open MTL file: {}", filepath);
            return materials;
        }

        Material* current = nullptr;
        std::string currentName;

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "newmtl")  // New material
            {
                iss >> currentName;
                materials[currentName] = Material();
                current = &materials[currentName];
            }
            else if (current)
            {
                if (prefix == "Ka")  // Ambient color
                {
                    iss >> current->ambient.x >> current->ambient.y >> current->ambient.z;
                }
                else if (prefix == "Kd")  // Diffuse color
                {
                    iss >> current->diffuse.x >> current->diffuse.y >> current->diffuse.z;
                }
                else if (prefix == "Ks")  // Specular color
                {
                    iss >> current->specular.x >> current->specular.y >> current->specular.z;
                }
                else if (prefix == "Ns")  // Shininess
                {
                    iss >> current->shininess;
                }
                else if (prefix == "map_Kd")  // Diffuse texture
                {
                    std::string texFile;
                    iss >> texFile;
                    std::string texPath = normalizePath(baseDir + texFile);
                    current->setDiffuseMap(AssetManager::get().loadTexture(texPath));
                }
                else if (prefix == "map_Ks")  // Specular texture
                {
                    std::string texFile;
                    iss >> texFile;
                    std::string texPath = normalizePath(baseDir + texFile);
                    current->setSpecularMap(AssetManager::get().loadTexture(texPath));
                }
                else if (prefix == "map_Bump" || prefix == "bump")  // Normal/bump map
                {
                    std::string texFile;
                    iss >> texFile;
                    std::string texPath = normalizePath(baseDir + texFile);
                    current->setNormalMap(AssetManager::get().loadTexture(texPath));
                }
            }
        }

        LOG_INFO("Loaded {} material(s) from {}", materials.size(), filepath);
        return materials;
    }
}