#pragma once
#include "math/EngineMath.h"
#include "scene/Material.h"
#include "renderer/interface/IMesh.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

/*
 * OBJLoader.h
 *
 * PURPOSE:
 * Minimal Wavefront OBJ/MTL parser for loading 3D models. Educational implementation
 * for learning file parsing, vertex deduplication, and material systems. **NOT production-
 * quality** - will migrate to tinyobjloader or Assimp (better performance, more
 * features, glTF support). Handles multi-material models, automatic triangulation, and
 * texture loading via AssetManager.
 *
 * DESIGN RATIONALE (October 13, 2025):
 * Problem: Need to load external 3D models (Blender exports). OBJ format industry-standard,
 * plain text (easy to parse). Need to understand file parsing, vertex indexing, material
 * systems. Production libraries (tinyobj, Assimp) too complex for learning.
 *
 * Solution: Minimal custom OBJ/MTL parser for educational purposes.
 * - OBJ parsing: Vertex data (positions, normals, UVs) + face definitions
 * - MTL parsing: Material properties + texture references
 * - Vertex deduplication: Hash map for identical vertices (40-60% memory savings)
 * - Multi-material support: Split into submeshes per material
 * - AssetManager integration: Texture caching with AF
 * - Future: Migrate to tinyobjloader for production
 *
 * Key Insight: Custom parser perfect for learning fundamentals. Understand OBJ format,
 * vertex indexing, material systems, file I/O. Production code should use battle-tested
 * libraries (tinyobj ~10× faster, handles edge cases, smoothing groups, etc.). This is
 * educational code, not production code.
 *
 * DESIGN PHILOSOPHY:
 * - Educational: Learn file parsing, not build production parser
 * - Minimal: Support common features, skip edge cases
 * - Temporary: Migrate to tinyobjloader/Assimp
 * - Functional: Works for common Blender exports
 * - Documented: Explain every step for learning
 *
 * KEY CONCEPTS:
 * 1. Wavefront OBJ Format:
 *    - Plain text, human-readable
 *    - v: Vertex position (v x y z)
 *    - vt: Texture coordinate (vt u v)
 *    - vn: Normal (vn x y z)
 *    - f: Face (f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3)
 *
 * 2. Vertex Deduplication:
 *    - OBJ references: Separate pos/tex/normal indices
 *    - GPU vertices: Interleaved pos+normal+uv
 *    - Deduplication: Hash map for identical vertices
 *    - Result: 40-60% memory savings (typical)
 *
 * 3. Multi-Material Splitting:
 *    - "usemtl" changes material
 *    - Each material = separate submesh
 *    - Result: Multiple ModelData per file
 *
 * 4. MTL Materials:
 *    - Ka: Ambient color
 *    - Kd: Diffuse color
 *    - Ks: Specular color
 *    - Ns: Shininess
 *    - map_Kd: Diffuse texture
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === LOAD SIMPLE MODEL ===
 * auto models = OBJLoader::load("assets/models/statue.obj");
 * for (auto& model : models) {
 *     auto obj = scene.createObject({model.mesh, model.material});
 *     obj->transform.position = vec3(0, 0, 0);
 * }
 * // Console: [INFO] Loaded OBJ: statue.obj (1 object, 2450 vertices)
 *
 * // === LOAD MULTI-MATERIAL MODEL ===
 * auto models = OBJLoader::load("assets/models/car.obj");
 * // Returns multiple ModelData (one per material)
 * for (auto& model : models) {
 *     LOG_INFO("Loaded: {} (material: {})", model.name, model.materialName);
 *     scene.createObject({model.mesh, model.material});
 * }
 * // Example output:
 * // [INFO] Loaded: CarBody (material: BodyMaterial)
 * // [INFO] Loaded: CarWheels (material: TireMaterial)
 * // [INFO] Loaded: CarGlass (material: GlassMaterial)
 * ```
 *
 * OBJ FORMAT - Structure:
 *
 * ```obj
 * # Cube.obj
 * mtllib Cube.mtl  # Material library
 *
 * # Vertex positions
 * v -1.0 -1.0 -1.0
 * v  1.0 -1.0 -1.0
 * v  1.0  1.0 -1.0
 * v -1.0  1.0 -1.0
 * # ... (8 vertices total)
 *
 * # Texture coordinates
 * vt 0.0 0.0
 * vt 1.0 0.0
 * vt 1.0 1.0
 * vt 0.0 1.0
 *
 * # Normals
 * vn 0.0 0.0 -1.0
 * vn 0.0 0.0  1.0
 * # ... (6 normals, one per face)
 *
 * # Object definition
 * o Cube
 *
 * # Material assignment
 * usemtl Material
 *
 * # Faces (v/vt/vn format)
 * f 1/1/1 2/2/1 3/3/1 4/4/1  # Quad (auto-triangulated)
 * # ... (6 faces total)
 * ```
 *
 * MTL FORMAT - Material Properties:
 *
 * ```mtl
 * # Cube.mtl
 * newmtl Material
 * Ka 0.2 0.2 0.2      # Ambient color (dark gray)
 * Kd 0.8 0.8 0.8      # Diffuse color (light gray)
 * Ks 1.0 1.0 1.0      # Specular color (white)
 * Ns 32.0             # Shininess (0-1000)
 * map_Kd diffuse.png  # Diffuse texture
 * map_Ks specular.png # Specular texture
 * map_Bump normal.png # Normal map
 * ```
 *
 * VERTEX DEDUPLICATION - How It Works:
 *
 * OBJ file:
 * ```obj
 * v 0 0 0   # Position 1
 * v 1 0 0   # Position 2
 * vt 0 0    # UV 1
 * vt 1 0    # UV 2
 * vn 0 1 0  # Normal 1
 *
 * f 1/1/1 2/2/1 1/1/1  # Triangle (reuses vertex 1)
 * ```
 *
 * Without deduplication (naive):
 * - 3 vertices: [(0,0,0),(0,0),(0,1,0)], [(1,0,0),(1,0),(0,1,0)], [(0,0,0),(0,0),(0,1,0)]
 * - GPU memory: 3 × 32 bytes = 96 bytes
 *
 * With deduplication (hash map):
 * - 2 unique vertices: [(0,0,0),(0,0),(0,1,0)], [(1,0,0),(1,0),(0,1,0)]
 * - Indices: [0, 1, 0]
 * - GPU memory: 2 × 32 bytes + 3 × 4 bytes = 76 bytes (21% savings)
 *
 * Typical savings: 40-60% for complex models
 *
 * DEDUPLICATION IMPLEMENTATION:
 *
 * ```cpp
 * struct OBJVertex {
 *     int posIndex, texIndex, normIndex;
 *     bool operator==(const OBJVertex& other) const { }
  * };
 * 
 * struct OBJVertexHash {
 *     size_t operator()(const OBJVertex& v) const {
 *         return hash(posIndex) ^ (hash(texIndex) << 1) ^ (hash(normIndex) << 2);
 *     }
 * };
 * 
 * std::unordered_map<OBJVertex, uint32_t, OBJVertexHash> vertexMap;
 * 
 * uint32_t getOrCreateVertex(ModelBuilder& builder, const OBJVertex& objVert) {
 *     // Check if vertex already exists
 *     auto it = vertexMap.find(objVert);
 *     if (it != vertexMap.end()) {
 *         return it->second;  // Reuse existing vertex
 *     }
 *     
 *     // Create new vertex
 *     uint32_t newIndex = builder.vertices.size();
 *     builder.vertices.push_back(createVertex(objVert));
 *     vertexMap[objVert] = newIndex;
 *     return newIndex;
 * }
 * ```
 * 
 * MULTI-MATERIAL SPLITTING:
 * 
 * OBJ with multiple materials:
 * ```obj
 * # Car.obj
 * mtllib Car.mtl
 * 
 * o CarBody
 * usemtl BodyMaterial     # Material 1 starts
 * f 1/1/1 2/2/1 3/3/1
 * # ... (body faces)
 * 
 * o CarWheels
 * usemtl TireMaterial     # Material 2 starts (split here)
 * f 100/50/20 101/51/20 102/52/20
 * # ... (wheel faces)
 * 
 * o CarGlass
 * usemtl GlassMaterial    # Material 3 starts (split here)
 * f 200/100/30 201/101/30 202/102/30
 * # ... (glass faces)
 * ```
 * 
 * Result:
 * - 3 ModelData objects (one per material)
 * - Each has separate Mesh + Material
 * - Rendered as separate draw calls (allows batching)
 * 
 * FACE PARSING - Triangulation:
 * 
 * ```cpp
 * void parseFace(std::istringstream& iss, ModelBuilder& builder) {
 *     std::vector<uint32_t> faceIndices;
 *     
 *     // Parse all vertices in face
 *     std::string token;
 *     while (iss >> token) {
 *         OBJVertex objVert = parseVertexRef(token);  // Parse "1/2/3"
 *         uint32_t index = getOrCreateVertex(builder, objVert);
 *         faceIndices.push_back(index);
 *     }
 *     
 *     // Triangulate (fan triangulation)
 *     // Triangle: 3 vertices -> 1 triangle
 *     // Quad:     4 vertices -> 2 triangles (0,1,2) + (0,2,3)
 *     // Pentagon: 5 vertices -> 3 triangles (0,1,2) + (0,2,3) + (0,3,4)
 *     for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
 *         builder.indices.push_back(faceIndices[0]);
 *         builder.indices.push_back(faceIndices[i]);
 *         builder.indices.push_back(faceIndices[i + 1]);
 *     }
 * }
 * ```
 * 
 * Fan triangulation (works for convex polygons):
 * - Quad ABCD -> Triangles ABC + ACD
 * - Pentagon ABCDE -> Triangles ABC + ACD + ADE
 * 
 * CURRENT STATE (October 13, 2025):
 * - OBJ parsing (positions, normals, UVs, faces)
 * - MTL parsing (materials, textures)
 * - Vertex deduplication (hash map, 40-60% savings)
 * - Multi-material support (submesh per material)
 * - Quad/polygon triangulation (fan method)
 * - Bounding volume calculation (AABB + sphere)
 * - AssetManager integration (texture caching)
 * 
 * CURRENT LIMITATIONS (By Design):
 * 
 * 1. No Smoothing Groups:
 * - OBJ "s on/off/number" ignored
 * - Uses face normals only
 * - Future: tinyobjloader handles this
 * 
 * 2. Fan Triangulation Only:
 * - Works for convex polygons
 * - Concave polygons may render incorrectly
 * - Future: Proper triangulation (earcut, Delaunay)
 * 
 * 3. No Free-Form Curves:
 * - NURBS not supported
 * - Future: Assimp supports this
 * 
 * 4. No Vertex Colors:
 * - Materials only
 * - Future: Parse vertex color extensions
 * 
 * 5. No Animation:
 * - Static models only
 * - Future: Assimp loads animations
 * 
 * 6. Limited Error Recovery:
 * - Malformed files may partially load
 * - Future: Robust error handling (tinyobj/Assimp)
 * 
 * MIGRATION PLAN:
 * 
 * Option 1: tinyobjloader (Recommended for OBJ)
 * - Single-header library (~5000 lines)
 * - 10× faster than custom parser
 * - Smoothing groups, proper triangulation
 * - Battle-tested (used in production)
 * - OBJ-specific (no glTF, FBX)
 * - Time: 1-2 days integration
 * 
 * Option 2: Assimp (Recommended for Multi-Format)
 * - 40+ formats (OBJ, FBX, glTF, COLLADA)
 * - Animation data loading
 * - Material PBR properties
 * - Mesh optimization (tangent generation, etc.)
 * - Larger dependency (~1MB library)
 * - Time: 3-5 days integration
 * 
 * Recommendation: Start with tinyobjloader (simple, OBJ-focused), add Assimp later
 * when glTF/FBX/animation support needed.
 * 
 * INTEGRATION WITH ROADMAP:
 * 
 * October 13, 2025: Initial implementation
 * - Custom OBJ/MTL parser (educational)
 * - Vertex deduplication (hash map)
 * - Multi-material support
 * - AssetManager integration
 * 
 * (Migration to tinyobjloader):
 * - Replace custom parser with tinyobjloader
 * - Add smoothing group support
 * - Improve triangulation (earcut)
 * - 10× faster loading
 * - Time: 1-2 days
 * 
 * (Migration to Assimp):
 * - Add FBX, glTF 2.0, COLLADA support
 * - Animation data loading
 * - Material PBR properties
 * - Mesh optimization (tangent generation)
 * - Time: 3-5 days
 * 
 * DEPENDENCIES:
 * - math/EngineMath.h: GLM wrapper (vec2/3, vertex struct)
 * - scene/Material.h: Material properties
 * - renderer/interface/IMesh.h: Mesh interface
 * - <vector>: Vertex/index storage
 * - <unordered_map>: Vertex deduplication, material lookup
 * 
 * THREAD SAFETY:
 * - NOT thread-safe: Static methods without mutex
 * - All operations on main thread only
 * - Future: Async loading with mutex protection
 * 
 * REFERENCES:
 * - Wavefront OBJ specification: http://paulbourke.net/dataformats/obj/
 * - tinyobjloader: https://github.com/tinyobjloader/tinyobjloader
 * - Assimp: https://github.com/assimp/assimp
 * 
 * HISTORY:
 * October 13, 2025: Initial implementation
 * - Custom OBJ/MTL parser (educational purposes)
 * - Vertex deduplication (hash map, 40-60% savings)
 * - Multi-material support (submesh per material)
 * - Quad/polygon triangulation (fan method)
 * - Bounding volume calculation (AABB + sphere)
 * - AssetManager integration (texture caching with AF)
 * - Result: Functional parser for learning file formats
 * 
 * Future: Migrate to tinyobjloader or Assimp
 * 
 */

namespace Engine
{
    class OBJLoader
    {
    public:
        struct ModelData
        {
            std::string name;
            std::shared_ptr<IMesh> mesh;
            Material material;
            std::string materialName;
        };

        /**
         * Load an OBJ file and return all objects with their materials
         */
        static std::vector<ModelData> load(const std::string& filepath);

    private:
        // Vertex data as stored in OBJ file (before indexing)
        struct OBJVertex
        {
            int posIndex = -1;
            int texIndex = -1;
            int normIndex = -1;

            bool operator==(const OBJVertex& other) const
            {
                return posIndex == other.posIndex &&
                    texIndex == other.texIndex &&
                    normIndex == other.normIndex;
            }
        };

        // Hash function for OBJVertex (for unordered_map)
        struct OBJVertexHash
        {
            size_t operator()(const OBJVertex& v) const
            {
                size_t h1 = std::hash<int>()(v.posIndex);
                size_t h2 = std::hash<int>()(v.texIndex);
                size_t h3 = std::hash<int>()(v.normIndex);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        // Temporary data while building a model
        struct ModelBuilder
        {
            std::string name;
            std::string materialName;
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            std::unordered_map<OBJVertex, uint32_t, OBJVertexHash> vertexMap;
        };

        /**
         * Parse a face line (f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3)
         */
        static void parseFace(
            std::istringstream& iss,
            ModelBuilder& builder,
            const std::vector<vec3>& positions,
            const std::vector<vec2>& texCoords,
            const std::vector<vec3>& normals);

        /**
         * Parse a vertex reference (e.g., "1/2/3" or "1//3" or "1/2" or "1")
         */
        static OBJVertex parseVertexRef(const std::string& token);

        /**
         * Get or create a vertex in the model builder
         */
        static uint32_t getOrCreateVertex(
            ModelBuilder& builder,
            const OBJVertex& objVertex,
            const std::vector<vec3>& positions,
            const std::vector<vec2>& texCoords,
            const std::vector<vec3>& normals);

        /**
         * Convert ModelBuilder to final ModelData with Mesh
         */
        static ModelData finalize(
            ModelBuilder& builder,
            const std::unordered_map<std::string, Material>& materials);

        /**
         * Load MTL (material) file
         */
        static std::unordered_map<std::string, Material> loadMTL(
            const std::string& filepath,
            const std::string& baseDir);

        /**
         * Helper functions
         */
        static std::string normalizePath(const std::string& path);
        static std::string getDirectory(const std::string& filepath);
        static std::string getBasename(const std::string& filepath);
    };
}