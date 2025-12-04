#pragma once
#include "scene/Transform.h"
#include "scene/Material.h"
#include "renderer/interface/IMesh.h"
#include "renderer/interface/IShader.h"
#include <vector>
#include <memory>

/*
 * SceneObject.h
 *
 * PURPOSE:
 * Represents a single renderable entity in the 3D scene. Combines transform (position/rotation/
 * scale), geometry (mesh), and appearance (material). Supports both single-material objects
 * (simple primitives) and multi-material objects (complex OBJ models with submeshes). Provides
 * world-space bounding volumes for frustum culling and collision detection.
 *
 * DESIGN RATIONALE (October 31, 2025):
 * Problem: SceneObject was nested inside Scene class (tight coupling). Adding multi-material
 * support for OBJ models required extending object structure, which didn't fit cleanly in
 * nested struct. Needed separation for clean architecture and reusability.
 *
 * Solution: Extract SceneObject to standalone class with submesh architecture.
 * - Separation of concerns: Scene = management, SceneObject = data
 * - Reusability: Can pass SceneObject* to RenderQueue, debug systems, etc.
 * - Extensibility: Multi-material support without changing Scene
 * - Interface abstraction: Uses IShader/IMesh for GL/Vulkan compatibility
 *
 * Key Insight: Object composition (transform + submeshes) is independent of scene management
 * (culling, batching, rendering order). Separating allows each system to focus on its
 * responsibility. Submesh architecture enables complex models with multiple materials while
 * keeping simple single-material API backwards compatible.
 *
 * DESIGN PHILOSOPHY:
 * - Composition over inheritance: Transform + Submeshes (no complex hierarchy)
 * - Public members: Direct access for convenience (struct-like)
 * - Shared mesh ownership: Memory efficiency via shared_ptr
 * - Value-based materials: Per-object customization via copy
 * - Multi-material support: Vector of submeshes (flexible)
 * - Backwards compatible: Single-material API still works
 *
 * KEY CONCEPTS:
 * 1. Submesh Architecture:
 *    - Submesh = Mesh + Material pair
 *    - Object has vector of submeshes (typically 1-10)
 *    - All submeshes share same transform (efficient)
 *    - Each submesh rendered independently (material batching)
 *
 * 2. Single vs Multi-Material:
 *    - Single: One submesh (most common, simple objects)
 *    - Multi: Multiple submeshes (OBJ models, complex objects)
 *    - API: Same for both (submeshes vector)
 *
 * 3. World-Space Bounding Volumes:
 *    - BoundingSphere: Fast frustum culling (6 plane tests)
 *    - AABB: Accurate collision detection (8 corner tests)
 *    - Both account for transform (position, rotation, scale)
 *
 * 4. Memory Sharing:
 *    - Mesh: shared_ptr (100 cubes = 1 mesh in memory)
 *    - Material: Copy (allows per-object customization)
 *    - Texture: shared_ptr in Material (cached by AssetManager)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === SINGLE-MATERIAL OBJECT (Backwards Compatible) ===
 * Material wood;
 * wood.setDiffuseMap(AssetManager::get().loadTexture("wood.jpg"));
 * wood.specular = vec3(0.3f);
 * wood.shininess = 8.0f;
 *
 * auto cube = scene.createObject(
 *     MeshFactory::createCube(1.0f),
 *     wood
 * );
 * cube->transform.position = vec3(0, 1, 0);
 * cube->transform.rotation = glm::quat(vec3(0, glm::radians(45.0f), 0));
 * cube->transform.scale = vec3(2, 1, 2);
 *
 * // === MULTI-MATERIAL OBJECT (OBJ Loader) ===
 * // OBJ file with multiple materials (car.obj has body, windows, tires)
 * auto carParts = OBJLoader::load("car.obj");
 * // Returns: vector<{mesh, material}> - one per material in OBJ
 *
 * auto car = scene.createObject(carParts);
 * car->transform.position = vec3(5, 0, 0);
 * // Each submesh batched separately:
 * // - Body (metal) batched with other metal objects
 * // - Windows (glass) batched with other transparent objects
 * // - Tires (rubber) batched with other black objects
 *
 * // === MANUAL MULTI-MATERIAL CONSTRUCTION ===
 * std::vector<SceneObject::Submesh> submeshes;
 *
 * Material metal;
 * metal.diffuse = vec3(0.7f, 0.7f, 0.7f);
 * metal.specular = vec3(1.0f);
 * metal.shininess = 128.0f;
 * submeshes.push_back({bodyMesh, metal});
 *
 * Material glass;
 * glass.diffuse = vec3(0.8f, 0.9f, 1.0f);
 * glass.alpha = 0.3f;
 * glass.isTransparent = true;
 * submeshes.push_back({windowsMesh, glass});
 *
 * Material rubber;
 * rubber.diffuse = vec3(0.1f, 0.1f, 0.1f);
 * rubber.specular = vec3(0.2f);
 * rubber.shininess = 8.0f;
 * submeshes.push_back({tiresMesh, rubber});
 *
 * auto car = scene.createObject(submeshes);
 * car->transform.position = vec3(10, 0, 0);
 *
 * // === BOUNDING VOLUME QUERIES ===
 * // Frustum culling (Scene does this automatically)
 * auto sphere = car->getWorldBoundingSphere();
 * if (frustum.testSphere(sphere.center, sphere.radius)) {
 *     car->render(shader);
 * }
 *
 * // Collision detection
 * auto playerAABB = player->getWorldAABB();
 * auto wallAABB = wall->getWorldAABB();
 * if (aabbs_intersect(playerAABB, wallAABB)) {
 *     handleCollision();
 * }
 * ```
 *
 * RENDERING FLOW:
 *
 * ```cpp
 * void SceneObject::render(IShader& shader) const {
 *     // Set model matrix once (shared by all submeshes)
 *     mat4 modelMatrix = transform.getMatrix();
 *     shader.setUniform("u_Model", modelMatrix);
 *
 *     // Render each submesh (material binds per submesh)
 *     for (const auto& submesh : submeshes) {
 *         submesh.material.bind(shader);  // Binds colors, textures, uniforms
 *         submesh.mesh->draw();            // Submits draw call
 *     }
 * }
 * ```
 *
 * Scene -> RenderQueue flow:
 * 1. Scene frustum culls objects (getWorldBoundingSphere)
 * 2. Visible objects submitted to RenderQueue
 * 3. RenderQueue sorts by material (first submesh material)
 * 4. RenderQueue calls object->render(shader)
 * 5. Object renders all submeshes sequentially
 *
 * BOUNDING VOLUME CALCULATION:
 *
 * Bounding Sphere (fast, conservative):
 * ```cpp
 * IMesh::BoundingSphere getWorldBoundingSphere() const {
 *     // Use first submesh sphere (conservative approximation)
 *     IMesh::BoundingSphere sphere = submeshes[0].mesh->boundingSphere;
 *
 *     // Transform to world space
 *     return sphere.toWorld(transform);  // Applies position, rotation, scale
 * }
 * ```
 * - Used by: Frustum culling (Scene::render)
 * - Cost: ~20 operations (fast)
 * - Conservative: May include objects slightly outside view (5% false positives)
 * - Validation: NaN detection, safe fallbacks
 *
 * AABB (accurate, precise):
 * ```cpp
 * IMesh::AABB getWorldAABB() const {
 *     // Union of all submesh AABBs
 *     IMesh::AABB result = submeshes[0].mesh->aabb.toWorld(transform);
 *
 *     for (size_t i = 1; i < submeshes.size(); i++) {
 *         IMesh::AABB subAABB = submeshes[i].mesh->aabb.toWorld(transform);
 *         result = union(result, subAABB);  // Expand to include all submeshes
 *     }
 *
 *     return result;
 * }
 * ```
 * - Used by: Collision detection, debug visualization
 * - Cost: ~50-100 operations per object (acceptable for collision)
 * - Accurate: Tightest axis-aligned box containing all geometry
 * - Multi-submesh: Union of all AABBs
 *
 * MEMORY MANAGEMENT:
 *
 * Ownership model:
 * - Scene owns SceneObject: unique_ptr (exclusive ownership)
 * - SceneObject shares Mesh: shared_ptr (multiple objects can reference)
 * - Material copied by value: Lightweight (~100 bytes)
 * - Textures shared in Material: shared_ptr (cached by AssetManager)
 *
 * Memory efficiency example:
 * ```cpp
 * // Load texture once (AssetManager caches)
 * auto woodTexture = AssetManager::get().loadTexture("wood.jpg");
 *
 * // Create material with texture
 * Material wood;
 * wood.setDiffuseMap(woodTexture);  // shared_ptr copy
 *
 * // Create 100 cubes sharing mesh and texture
 * auto cubeMesh = MeshFactory::createCube(1.0f);
 * for (int i = 0; i < 100; i++) {
 *     auto cube = scene.createObject(cubeMesh, wood);  // Mesh shared, material copied
 *     cube->transform.position = vec3(i * 2, 0, 0);
 * }
 *
 * // Memory usage:
 * // - 1 wood.jpg texture in GPU (cached)
 * // - 1 cube mesh in GPU (shared via shared_ptr)
 * // - 100 Material copies in CPU (~10 KB)
 * // - 100 SceneObjects (~20 KB)
 * // Total: ~30 KB CPU + minimal GPU (vs 400+ MB if duplicated)
 * //
 * // RenderQueue batching:
 * // - All 100 cubes have identical materials
 * // - operator< groups them together
 * // - Result: 1 material bind for all 100 cubes!
 * ```
 *
 * VALIDATION AND SAFETY:
 *
 * Bounding sphere calculation includes comprehensive validation:
 * ```cpp
 * IMesh::BoundingSphere getWorldBoundingSphere() const {
 *     // Check for empty submeshes
 *     if (submeshes.empty()) {
 *         LOG_ERROR("SceneObject has no submeshes");
 *         return {transform.position, 0.0f};  // Zero sphere at object position
 *     }
 *
 *     // Check for null mesh
 *     if (!submeshes[0].mesh) {
 *         LOG_ERROR("SceneObject first submesh has null mesh");
 *         return {transform.position, 0.0f};
 *     }
 *
 *     // Transform to world space
 *     IMesh::BoundingSphere sphere = submeshes[0].mesh->boundingSphere.toWorld(transform);
 *
 *     // Validate result (detect NaN, Inf, unreasonable values)
 *     if (std::isnan(sphere.center.x) || std::isnan(sphere.radius)) {
 *         LOG_ERROR("Invalid bounding sphere (NaN) at {}", transform.position);
 *         return {transform.position, 10.0f};  // Safe fallback
 *     }
 *
 *     if (sphere.radius > 1000.0f) {
 *         LOG_WARN("Unusually large bounding sphere (radius={}) at {}",
 *                  sphere.radius, transform.position);
 *     }
 *
 *     return sphere;
 * }
 * ```
 *
 * Why validation matters:
 * - Invalid transforms (zero scale, NaN) produce garbage bounds
 * - Uninitialized data can contain random values
 * - Without validation: Frustum culling crashes or artifacts
 * - With validation: Safe fallbacks keep rendering while logging issues
 *
 * ARCHITECTURE EVOLUTION:
 *
 * Before (October 7 - October 30, 2025): Nested in Scene
 * ```cpp
 * class Scene {
 *     struct SceneObject {  // Nested, tightly coupled
 *         Transform transform;
 *         std::shared_ptr<Mesh> mesh;
 *         Material material;
 *     };
 *     std::vector<SceneObject> m_objects;
 * };
 * ```
 * Problems: Can't reuse outside Scene, hard to extend, tight coupling
 *
 * After (October 31, 2025+): Standalone class
 * ```cpp
 * class SceneObject {  // Independent, reusable
 *     Transform transform;
 *     std::vector<Submesh> submeshes;
 * };
 *
 * class Scene {
 *     std::vector<std::unique_ptr<SceneObject>> m_objects;
 * };
 * ```
 * Benefits: Separation of concerns, multi-material support, reusable
 *
 * CURRENT LIMITATIONS (By Design, Address Later):
 *
 * 1. No Parent-Child Hierarchy:
 * - Objects can't be attached to each other (car wheels to body)
 * - Future: Scene graph with transform hierarchy (Week 7-8)
 *
 * 2. Sphere Uses First Submesh Only:
 * - Multi-submesh objects may have loose culling bounds
 * - Future: Union of all submesh spheres (Week 4)
 *
 * 3. No Per-Submesh Culling:
 * - All submeshes rendered if object visible (all-or-nothing)
 * - Future: Individual frustum tests per submesh (optimization)
 *
 * 4. No Visibility Flags:
 * - Can't hide objects without removing from scene
 * - Future: isVisible flag (Week 5+)
 *
 * 5. No Object Naming/Tagging:
 * - Can't find objects by name (findByName("Player"))
 * - Future: Name and tag system (Week 5+)
 *
 * 6. Deprecated Material Member:
 * - Public `material` exists for backwards compatibility
 * - Should use submeshes[0].material instead
 * - Future: Remove deprecated member (breaking change)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 31, 2025: Extraction from Scene
 * - Separated SceneObject to standalone class
 * - Added multi-material submesh architecture
 * - Added interface abstraction (IShader, IMesh)
 * - Status: Complete, production-ready
 *
 * (Scene Hierarchy):
 * - Parent-child transform relationships
 * - Attach objects to each other
 * - Time: 1-2 days
 *
 * (Object Queries):
 * - Naming and tagging system
 * - findByName, findByTag methods
 * - Time: 1 day
 *
 * DEPENDENCIES:
 * - scene/Transform.h: Position, rotation, scale
 * - scene/Material.h: Appearance properties
 * - renderer/interface/IMesh.h: Geometry interface
 * - renderer/interface/IShader.h: Shader interface
 * - <vector>: Submesh storage
 * - <memory>: shared_ptr for mesh sharing
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Transform modification during rendering causes artifacts
 * - Shared mesh: Thread-safe (read-only after creation)
 * - Material copy: Thread-safe (each object has own copy)
 * - All SceneObject operations on main thread only
 *
 * REFERENCES:
 * - Game Engine Architecture 3rd Ed., Chapter 14: Scene graphs
 * - refactor: Separation of concerns principle
 * - OBJ file format: Multi-material model structure
 *
 * HISTORY:
 * October 7-30, 2025: Nested in Scene class
 * - Basic SceneObject struct inside Scene
 * - Single mesh + material only
 * - Tight coupling with Scene
 *
 * October 31, 2025: Extraction to standalone class
 * - Moved to separate SceneObject.h file
 * - Added submesh architecture (multi-material support)
 * - Added world-space bounding volume methods
 * - Added interface abstraction (IShader, IMesh)
 * - Added comprehensive validation
 * - Result: Clean architecture, OBJ model support
 *
 */

namespace Engine
{
    class SceneObject
    {
    public:
        // Submesh: One mesh + material pair
        struct Submesh
        {
            std::shared_ptr<IMesh> mesh;
            Material material;
        };

        // Public members for direct access
        Transform transform;
        Material material;  // Deprecated: Use submeshes[0].material
        std::vector<Submesh> submeshes;

        // Constructor for single-material objects (backwards compatible)
        SceneObject(std::shared_ptr<IMesh> mesh, const Material& mat);

        // Constructor for multi-material objects
        SceneObject(const std::vector<Submesh>& subs);

        // Rendering
        void render(IShader& shader) const;  // Changed from Shader& to IShader&

        // Submesh access
        size_t getSubmeshCount() const { return submeshes.size(); }
        Submesh& getSubmesh(size_t index) { return submeshes[index]; }
        const Submesh& getSubmesh(size_t index) const { return submeshes[index]; }

        // World-space bounding volumes
        IMesh::BoundingSphere getWorldBoundingSphere() const;
        IMesh::AABB getWorldAABB() const;

        // Mesh access (returns first submesh mesh)
        std::shared_ptr<IMesh> getMesh() const;
    };
}