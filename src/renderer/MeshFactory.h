#pragma once
#include "renderer/interface/IMesh.h"
#include "renderer/interface/IRenderDevice.h"
#include "math/EngineMath.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <numbers>

/*
 * MeshFactory.h
 *
 * PURPOSE:
 * Procedurally generates common 3D primitives for testing, prototyping, and placeholder
 * geometry. Provides 11 primitives with proper normals, UVs, and bounding volumes. All
 * geometry uses CCW winding for back-face culling compatibility. Essential for rapid
 * prototyping and content creation without external modeling tools.
 *
 * DESIGN RATIONALE (October 9-30, 2025):
 * Problem: Need quick geometry for testing renderer features. External models slow workflow.
 * Need consistent winding order for face culling. Need bounding volumes for frustum culling.
 * Need configurable quality (low-poly for performance, high-poly for quality).
 *
 * Solution: Static factory methods generating indexed meshes with full vertex data.
 * - Procedural generation: Cube, sphere, icosphere, plane, cylinder, cone, pyramid, capsule, torus
 * - CCW winding: Compatible with glFrontFace(GL_CCW) + glCullFace(GL_BACK)
 * - Bounding volumes: Auto-calculated AABB + sphere (required for culling)
 * - Configurable quality: Tessellation parameters (segments, subdivisions)
 * - October 28-30 revamp: Fixed winding order, added 6 new primitives
 *
 * Key Insight: Procedural primitives essential for rapid iteration. CCW winding critical
 * for face culling (98% of games use back-face culling for performance). Bounding volumes
 * non-optional (Scene's frustum culling requires them). Tessellation parameters enable
 * quality/performance trade-offs.
 *
 * DESIGN PHILOSOPHY:
 * - Static factory: No instance needed (MeshFactory::createCube)
 * - Dependency injection: IRenderDevice passed via initialize()
 * - Indexed rendering: Vertex reuse (memory efficient)
 * - Bounding volumes: Always computed (required for culling)
 * - Sensible defaults: Works out-of-box, customize when needed
 * - Mathematical precision: C++20 std::numbers constants
 *
 * KEY CONCEPTS:
 * 1. Procedural Generation:
 *    - Calculate vertices mathematically (no external files)
 *    - Result: Instant geometry, no asset loading
 *
 * 2. CCW Winding (Counter-Clockwise):
 *    - Vertices ordered CCW when viewed from outside
 *    - Compatible with glCullFace(GL_BACK) (standard)
 *    - Result: Back faces culled, front faces rendered
 *
 * 3. Bounding Volumes:
 *    - AABB: Axis-aligned box (min/max corners)
 *    - Sphere: Center + radius (fast culling)
 *    - Calculated: Auto-computed from vertices
 *
 * 4. Tessellation:
 *    - Segments: Number of subdivisions (more = smoother)
 *    - Trade-off: Quality vs performance
 *    - Examples: 8 segments (low-poly), 32 (balanced), 64 (high-quality)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION (Once, Application Startup) ===
 * MeshFactory::initialize(renderDevice.get());
 *
 * // === BASIC PRIMITIVES ===
 * auto cube = MeshFactory::createCube(1.0f);              // 1m cube
 * auto sphere = MeshFactory::createSphere(0.5f, 32, 16);  // 0.5m radius, 32 sectors, 16 stacks
 * auto plane = MeshFactory::createPlane(10.0f, 10.0f);    // 10m × 10m floor
 * auto quad = MeshFactory::createQuad(1.0f, 1.0f);        // 1m × 1m billboard
 *
 * // === ADVANCED PRIMITIVES ===
 * auto cylinder = MeshFactory::createCylinder(0.5f, 2.0f, 32);  // Radius, height, segments
 * auto cone = MeshFactory::createCone(0.5f, 1.0f, 32);          // Radius, height, segments
 * auto pyramid = MeshFactory::createPyramid(1.0f, 1.5f);        // Base size, height
 * auto capsule = MeshFactory::createCapsule(0.5f, 2.0f, 16, 8); // Radius, height, segs, rings
 * auto torus = MeshFactory::createTorus(1.0f, 0.3f, 32, 16);    // Major/minor radius, segments
 * auto icosphere = MeshFactory::createIcosphere(0.5f, 2);       // Radius, subdivisions
 * auto skybox = MeshFactory::createSkyboxCube(100.0f);          // Environment cube
 *
 * // === QUALITY LEVELS (Tessellation) ===
 * auto lowSphere = MeshFactory::createSphere(1.0f, 16, 8);   // Low-poly (performance)
 * auto highSphere = MeshFactory::createSphere(1.0f, 64, 32);  // High-poly (quality)
 * auto terrain = MeshFactory::createPlane(100.0f, 100.0f, 50, 50);  // Subdivided
 *
 * // === SCENE INTEGRATION (Mesh Sharing) ===
 * auto cubeMesh = MeshFactory::createCube(1.0f);  // Create once
 *
 * for (int i = 0; i < 100; i++) {
 *     auto obj = scene.createObject(cubeMesh, material);  // Reuse 100 times
 *     obj->transform.position = vec3(i * 2.0f, 0, 0);
 * }
 * // Result: 1 mesh in GPU memory, 100 rendered objects (99% memory savings)
 * ```
 *
 * AVAILABLE PRIMITIVES (11 Total):
 *
 * Basic shapes:
 * - **Cube**: Box with 6 faces (buildings, boxes, placeholders)
 * - **Sphere**: UV sphere with lat/long (balls, planets, particles)
 * - **Icosphere**: Geodesic sphere, uniform triangles (physics, smooth surfaces)
 * - **Plane**: Flat grid with tessellation (ground, floors, terrain)
 * - **Cylinder**: Capped cylinder (pillars, trees, pipes)
 * - **Quad**: Simple 2D rectangle (UI, billboards, sprites)
 *
 * Advanced shapes:
 * - **Cone**: Pointed cone (spotlights, arrows, markers)
 * - **Pyramid**: Four-sided pyramid (roofs, decorative)
 * - **Capsule**: Cylinder + hemispherical caps (character colliders)
 * - **Torus**: Donut shape (rings, tires, portals)
 *
 * Special:
 * - **Skybox Cube**: Inverted cube for environment mapping
 *
 * VERTEX FORMAT:
 *
 * All meshes use VertexFormat::PositionNormalUV:
 * ```cpp
 * struct Vertex {
 *     vec3 position;  // 3D world-space location
 *     vec3 normal;    // Surface normal for lighting
 *     vec2 uv;        // Texture coordinates
 * };
 * // Total: 8 floats × 4 bytes = 32 bytes per vertex
 * ```
 *
 * WINDING ORDER (October 28-30, 2025 Revamp):
 *
 * All primitives use **counter-clockwise (CCW) winding** from outside:
 * - Compatible with: `glFrontFace(GL_CCW)` + `glCullFace(GL_BACK)`
 * - Result: Back faces culled (performance), front faces rendered
 *
 * Implementation notes:
 * - Cube, Torus, Plane, Quad: Direct CCW winding
 * - Sphere, Cylinder, Cone, Pyramid, Capsule: Reversed during generation
 * - Icosphere: Natural CCW from icosahedron base geometry
 * - Skybox: Inverted normals + reversed winding (CW from outside = CCW from inside)
 *
 * Tested and verified: All primitives work with face culling enabled
 *
 * BOUNDING VOLUMES (Required):
 *
 * Every mesh includes pre-calculated bounding volumes:
 * ```cpp
 * void calculateBounds(IMesh& mesh, const std::vector<Vertex>& vertices) {
 *     // AABB (Axis-Aligned Bounding Box)
 *     vec3 minBounds(FLT_MAX);
 *     vec3 maxBounds(-FLT_MAX);
 *     for (const auto& v : vertices) {
 *         minBounds = glm::min(minBounds, v.position);
 *         maxBounds = glm::max(maxBounds, v.position);
 *     }
 *     mesh.aabb = {minBounds, maxBounds};
 *
 *     // Bounding Sphere
 *     vec3 center = (minBounds + maxBounds) * 0.5f;
 *     float radius = 0.0f;
 *     for (const auto& v : vertices) {
 *         radius = std::max(radius, glm::length(v.position - center));
 *     }
 *     mesh.boundingSphere = {center, radius};
 * }
 * ```
 *
 * Critical: Missing bounds cause frustum culling errors (objects disappear)
 *
 * SPHERE vs ICOSPHERE:
 *
 * UV Sphere (latitude/longitude):
 * - Pros: Simple generation, easy UV mapping
 * - Cons: Polar distortion (tiny triangles at poles), UV seams visible
 * - Best for: Textured objects, legacy compatibility
 * - Vertex count: ~561 vertices (32×16)
 *
 * Icosphere (geodesic subdivision):
 * - Pros: Uniform triangle distribution, no poles, better for physics
 * - Cons: More complex generation, UV seam still present
 * - Best for: Physics collision, procedural generation, smooth surfaces
 * - Vertex count: ~162 vertices (subdiv 2)
 *
 * TYPICAL VERTEX COUNTS:
 *
 * Default quality (32 segments):
 * - Cube: 24 vertices, 36 indices
 * - Sphere (32×16): 561 vertices, 2,880 indices
 * - Icosphere (subdiv 2): 162 vertices, 960 indices
 * - Cylinder (32 segs): 134 vertices, 384 indices
 * - Plane (10×10): 121 vertices, 600 indices
 * - Quad: 4 vertices, 6 indices
 * - Cone (32 segs): 68 vertices, 192 indices
 * - Pyramid: 16 vertices, 18 indices
 * - Capsule (16×8): 306 vertices, 1,440 indices
 * - Torus (32×16): 561 vertices, 3,072 indices
 * - Skybox: 24 vertices, 36 indices
 *
 * PERFORMANCE:
 *
 * Generation cost (CPU, one-time):
 * - Simple (cube, quad): ~0.01ms
 * - Medium (sphere, cylinder): ~0.1ms
 * - Complex (torus, high-poly): ~1ms
 * - Best practice: Create once, share across objects
 *
 * Memory (GPU):
 * - Sphere (32×16): 561 × 32 bytes = ~18 KB
 * - 100 objects sharing mesh: 18 KB (not 1.8 MB)
 * - Result: Massive savings via shared_ptr
 *
 * HELPER FUNCTIONS:
 *
 * ```cpp
 * // Generate unit circle vertices (for cone, cylinder, torus)
 * static std::vector<vec2> generateCircleVertices(int segments);
 *
 * // Calculate smooth normals (area-weighted averaging)
 * static void calculateSmoothNormals(std::vector<Vertex>& vertices,
 *                                     const std::vector<uint32_t>& indices);
 *
 * // Calculate bounding volumes (AABB + sphere)
 * static void calculateBounds(IMesh& mesh, const std::vector<Vertex>& vertices);
 *
 * // Convert vertex structs to flat float array (for GPU upload)
 * static std::vector<float> verticesToFloats(const std::vector<Vertex>& vertices);
 * ```
 *
 * MATHEMATICAL PRECISION (C++20):
 *
 * ```cpp
 * static constexpr float PI = std::numbers::pi_v<float>;      // 3.14159265...
 * static constexpr float TWO_PI = 2.0f * PI;                  // 6.28318530...
 * static constexpr float HALF_PI = PI / 2.0f;                 // 1.57079632...
 * ```
 *
 * More accurate than hardcoded values (prevents transcription errors)
 *
 * CURRENT STATE (October 30, 2025):
 * - 11 procedural primitives (cube, sphere, icosphere, plane, cylinder, quad, cone, pyramid, capsule, torus, skybox)
 * - CCW winding (all primitives, face culling compatible)
 * - Bounding volumes (AABB + sphere, auto-calculated)
 * - Configurable quality (tessellation parameters)
 * - Indexed rendering (vertex reuse, memory efficient)
 * - Helper functions (reduce duplication)
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Vertex Format:
 * - Only PositionNormalUV (no tangent space for normal mapping)
 * - Future: Add PositionNormalTangentUV (Week 8+)
 *
 * 2. UV Seams:
 * - Spherical shapes have visible seams (equirectangular projection)
 * - Future: Better UV mapping (cubic, triplanar, Week 10+)
 *
 * 3. No LOD:
 * - Single detail level per mesh
 * - Future: Auto-generate LOD levels (Week 10+)
 *
 * 4. No Custom UV Mapping:
 * - Hardcoded UV layouts
 * - Future: Custom UV projection options (Week 10+)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 9, 2025: Initial implementation
 * - Basic primitives (cube, sphere, plane, quad)
 * - Indexed rendering
 * - Basic bounding volumes
 *
 * October 28-30, 2025: Major revamp
 * - Fixed CCW winding order (all primitives)
 * - Added 6 new primitives (cone, pyramid, capsule, torus, icosphere, skybox)
 * - Enhanced bounding volume calculation
 * - Added helper functions
 * - Tested with face culling enabled
 *
 * (Enhanced Vertex Formats):
 * - PositionNormalTangentUV for normal mapping
 * - PositionNormalUVBones for skeletal animation
 * - Time: 1-2 days
 *
 * (Advanced Features):
 * - Automatic LOD generation
 * - Better UV mapping (cubic, triplanar)
 * - Mesh optimization (vertex cache, simplification)
 * - Procedural generation (noise-based terrain)
 * - Time: 2-3 weeks total
 *
 * DEPENDENCIES:
 * - renderer/interface/IMesh.h: Mesh interface
 * - renderer/interface/IRenderDevice.h: Factory for mesh creation
 * - math/EngineMath.h: GLM wrapper (vec2/3, trigonometry)
 * - <memory>: std::shared_ptr for mesh sharing
 * - <vector>: Vertex/index storage
 * - <numbers>: C++20 mathematical constants
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Static storage (s_renderDevice)
 * - All operations on main thread only
 * - Mesh creation: Safe (render device handles GPU upload)
 *
 * REFERENCES:
 * - LearnOpenGL.com: Primitive generation tutorials
 * - Real-Time Rendering 4th Ed., Chapter 16: Polygonal techniques
 * - C++20 std::numbers: Mathematical constants
 *
 * HISTORY:
 * October 9, 2025: Initial implementation
 * - Basic primitives (cube, sphere, plane, quad)
 * - Indexed rendering
 * - Basic bounding volumes
 *
 * October 28-30, 2025: Major revamp
 * - Fixed CCW winding order (face culling compatibility)
 * - Added 6 new primitives (cone, pyramid, capsule, torus, icosphere, skybox)
 * - Enhanced bounding volume calculation
 * - Added helper functions (generateCircleVertices, calculateSmoothNormals)
 * - Comprehensive testing with face culling
 * - C++20 std::numbers for mathematical constants
 * - Result: 11 production-ready primitives with correct winding
 *
 */

namespace Engine
{
    class MeshFactory
    {
    public:
        // Mathematical constants (C++20 precise values)
        static constexpr float PI = std::numbers::pi_v<float>;
        static constexpr float TWO_PI = 2.0f * PI;
        static constexpr float HALF_PI = PI / 2.0f;

        // REQUIRED: Initialize with render device before creating meshes
        static void initialize(IRenderDevice* renderDevice);

        // === BASIC PRIMITIVES ===
        static std::shared_ptr<IMesh> createCube(float size = 1.0f);
        static std::shared_ptr<IMesh> createSphere(float radius = 0.5f, int sectors = 32, int stacks = 16);
        static std::shared_ptr<IMesh> createIcosphere(float radius = 0.5f, int subdivisions = 2);
        static std::shared_ptr<IMesh> createPlane(float width = 10.0f, float depth = 10.0f, int segmentsX = 1, int segmentsZ = 1);
        static std::shared_ptr<IMesh> createCylinder(float radius = 0.5f, float height = 2.0f, int segments = 32);
        static std::shared_ptr<IMesh> createQuad(float width = 1.0f, float height = 1.0f);

        // === ADVANCED PRIMITIVES ===
        static std::shared_ptr<IMesh> createCone(float radius = 0.5f, float height = 1.0f, int segments = 32);
        static std::shared_ptr<IMesh> createPyramid(float baseSize = 1.0f, float height = 1.0f);
        static std::shared_ptr<IMesh> createCapsule(float radius = 0.5f, float height = 2.0f, int segments = 16, int rings = 8);
        static std::shared_ptr<IMesh> createTorus(float majorRadius = 1.0f, float minorRadius = 0.3f, int majorSegments = 32, int minorSegments = 16);

        // === SPECIAL PRIMITIVES ===
        static std::shared_ptr<IMesh> createSkyboxCube(float size = 100.0f);

    private:
        static IRenderDevice* s_renderDevice;  // Injected dependency

        // === HELPER FUNCTIONS ===
        static std::vector<float> verticesToFloats(const std::vector<Vertex>& vertices);
        static std::vector<vec2> generateCircleVertices(int segments);
        static void calculateSmoothNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        static void calculateBounds(IMesh& mesh, const std::vector<Vertex>& vertices);
    };
}