#pragma once
#include "renderer/interface/IDebugRenderer.h"
#include "renderer/interface/IRenderer.h"
#include "renderer/opengl/GLShader.h"
#include <glad/glad.h>
#include <memory>
#include <vector>

/*
 * GLDebugRenderer.h
 *
 * PURPOSE:
 * Immediate-mode debug visualization for development and testing. Draws lines, shapes, and
 * coordinate axes without persistent meshes. Essential for debugging physics, AI, collision,
 * frustum culling. Implements IDebugRenderer interface with static backend for global access.
 * One-frame lifetime (auto-clear after render), batched draw calls (efficient).
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Need quick visualization during development (bounding volumes, AI paths, collision
 * points). Creating meshes too slow/complex. Need immediate-mode API (call drawLine, see line).
 * Need one-frame lifetime (prevent stale debug data). Need efficient batching (hundreds of
 * lines per frame acceptable).
 *
 * Solution: Static immediate-mode renderer with line batching.
 * - Immediate mode: drawLine() accumulates during update, render() draws all at once
 * - Static backend: Global access via static methods
 * - One-frame lifetime: clear() after render() removes all geometry
 * - Batching: All lines uploaded once, single draw call
 * - Unlit: Solid colors, not affected by scene lighting
 *
 * Key Insight: Immediate mode perfect for debug visualization. Convenience over performance.
 * Batching mitigates performance cost - 500 lines = one draw call. Static access everywhere
 * (no passing renderer around). One-frame lifetime prevents confusion (fresh each frame).
 *
 * DESIGN PHILOSOPHY:
 * - Immediate mode: Call draw functions, see results (no mesh management)
 * - Static access: Global convenience (DebugRenderer::drawLine anywhere)
 * - One-frame lifetime: Auto-clear prevents stale debug data
 * - Batched rendering: Efficient despite immediate mode
 * - Unlit shading: Clear colors, always visible
 * - Optional depth test: X-ray mode (see through objects)
 *
 * KEY CONCEPTS:
 * 1. Immediate Mode:
 *    - Draw calls accumulate lines during update loop
 *    - render() uploads all lines, draws with single call
 *    - clear() removes all lines (ready for next frame)
 *
 * 2. Static Backend:
 *    - Static storage: s_lines vector, s_shader, s_vao, s_vbo
 *    - Instance methods: Forward to static implementations
 *    - Result: Can use via interface OR static methods
 *
 * 3. Line Batching:
 *    - Lines stored in vector during update (CPU)
 *    - render(): Upload all lines to GPU (one glBufferData)
 *    - Single glDrawArrays renders all lines (efficient)
 *
 * 4. Depth Testing:
 *    - Enabled: Lines occluded by scene geometry
 *    - Disabled: Lines always visible (X-ray mode, default)
 *    - Toggle: setDepthTest(true/false)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === BASIC VISUALIZATION ===
 * void onUpdate(float dt) {
 *     // Visualize object positions (red spheres)
 *     for (auto& obj : objects) {
 *         DebugRenderer::drawSphere(obj.position, 0.5f, vec3(1, 0, 0));
 *     }
 *
 *     // Show AI patrol path (cyan lines)
 *     for (size_t i = 0; i < waypoints.size() - 1; i++) {
 *         DebugRenderer::drawLine(waypoints[i], waypoints[i+1], vec3(0, 1, 1));
 *     }
 *
 *     // Draw world axes at origin (RGB = XYZ)
 *     DebugRenderer::drawAxes(vec3(0, 0, 0), 5.0f);
 * }
 *
 * void onRender() {
 *     // Render scene
 *     scene.render(camera, shader, window, renderer);
 *
 *     // Render debug overlays
 *     DebugRenderer::render(camera, window, renderer);
 *
 *     // Auto-clear for next frame
 *     DebugRenderer::clear();
 * }
 *
 * // === BOUNDING VOLUMES ===
 * void debugBoundingVolumes(Scene& scene) {
 *     for (auto& obj : scene.getObjects()) {
 *         // Bounding sphere (cyan, rotation-invariant)
 *         auto sphere = obj->getWorldBoundingSphere();
 *         DebugRenderer::drawSphere(sphere.center, sphere.radius, vec3(0, 1, 1));
 *
 *         // AABB (yellow, grows/shrinks on rotation)
 *         auto aabb = obj->getWorldAABB();
 *         DebugRenderer::drawBox(aabb.getCenter(), aabb.getExtents(), vec3(1, 1, 0));
 *     }
 * }
 *
 * // === ORIENTED BOUNDING BOXES (Week 4) ===
 * void debugOBBs(Scene& scene) {
 *     for (auto& obj : scene.getObjects()) {
 *         // Get local AABB
 *         auto aabb = obj->getMesh()->aabb;
 *         vec3 modelCenter = (aabb.min + aabb.max) * 0.5f;
 *         vec3 halfExtents = (aabb.max - aabb.min) * 0.5f;
 *
 *         // Transform to world space
 *         mat4 rotation = obj->transform.getRotationMatrix();
 *         vec3 worldCenter = obj->transform.position +
 *                            vec3(rotation * vec4(modelCenter * obj->transform.scale, 0.0f));
 *
 *         // Draw OBB (magenta, rotates with object, tight fit)
 *         DebugRenderer::drawOBB(
 *             worldCenter,
 *             halfExtents * obj->transform.scale,
 *             rotation,
 *             vec3(1, 0.5f, 1)  // Magenta
 *         );
 *     }
 * }
 *
 * // === CAMERA FRUSTUM (Week 4) ===
 * void debugCameraFrustum(Camera& camera, Window& window) {
 *     // Visualize what camera sees (yellow pyramid)
 *     mat4 viewProj = camera.getViewProjectionMatrix(window.getAspectRatio());
 *     DebugRenderer::drawFrustum(viewProj, vec3(1, 1, 0));
 *
 *     // Note: Viewing active camera frustum shows yellow border
 *     // (you're inside pyramid looking outward)
 *     // Use secondary camera to see frustum from outside
 * }
 *
 * // === MULTI-CAMERA FRUSTUMS ===
 * void debugMultipleCameras(Camera& mainCam, Camera& debugCam, Window& window) {
 *     // Main camera frustum (yellow)
 *     mat4 mainVP = mainCam.getViewProjectionMatrix(window.getAspectRatio());
 *     DebugRenderer::drawFrustum(mainVP, vec3(1, 1, 0));
 *
 *     // Shadow camera frustum (red)
 *     mat4 shadowVP = shadowCamera.getViewProjectionMatrix(1.0f);
 *     DebugRenderer::drawFrustum(shadowVP, vec3(1, 0, 0));
 *
 *     // View from debug camera (see both frustums)
 *     DebugRenderer::render(debugCam, window, renderer);
 * }
 *
 * // === PHYSICS DEBUGGING ===
 * void debugPhysics() {
 *     // Visualize velocity (magenta arrow)
 *     DebugRenderer::drawArrow(
 *         player.position,
 *         player.position + player.velocity,
 *         vec3(1, 0, 1),  // Magenta
 *         0.3f            // Arrow tip size
 *     );
 *
 *     // Show collision points (red spheres)
 *     for (auto& contact : collisionContacts) {
 *         DebugRenderer::drawSphere(contact.point, 0.1f, vec3(1, 0, 0));
 *     }
 *
 *     // Visualize oriented collision boxes (green)
 *     for (auto& collider : rotatedColliders) {
 *         DebugRenderer::drawOBB(
 *             collider.center,
 *             collider.halfExtents,
 *             collider.rotation,
 *             vec3(0, 1, 0)
 *         );
 *     }
 * }
 *
 * // === RENDERING MODES ===
 * // X-ray mode (default, see through objects)
 * DebugRenderer::setDepthTest(false);
 *
 * // Respect scene depth (occluded by objects)
 * DebugRenderer::setDepthTest(true);
 *
 * // Thicker lines (easier to see)
 * DebugRenderer::setLineWidth(2.0f);
 * ```
 *
 * AVAILABLE PRIMITIVES:
 *
 * Basic shapes:
 * - `drawLine(start, end, color)`: Single line segment
 * - `drawSphere(center, radius, color, segments=16)`: Wireframe sphere (3 circles)
 * - `drawBox(center, halfSize, color)`: Wireframe AABB (12 lines)
 * - `drawArrow(start, end, color, tipSize=0.2)`: Line with cone tip
 * - `drawGrid(size, divisions, color)`: Ground plane grid (XZ plane)
 *
 * Advanced shapes (Week 4):
 * - `drawOBB(center, halfSize, rotation, color)`: Wireframe OBB (rotates with object)
 * - `drawFrustum(viewProj, color)`: Camera view frustum (pyramid, 12 lines)
 *
 * Coordinate frames:
 * - `drawAxes(position, size=1.0)`: RGB axes at position (X=red, Y=green, Z=blue)
 * - `drawAxes(transform, size=1.0)`: RGB axes from transform (rotates with object)
 *
 * BATCHING IMPLEMENTATION:
 *
 * ```cpp
 * // Storage
 * struct DebugLine {
 *     vec3 start;   // Start position
 *     vec3 color;   // RGB color
 *     vec3 end;     // End position
 *     vec3 padding; // Alignment (48 bytes total)
 * };
 * static std::vector<DebugLine> s_lines;  // Accumulated lines
 *
 * // Accumulate (called during update)
 * void staticDrawLine(const vec3& start, const vec3& end, const vec3& color) {
 *     s_lines.push_back({start, color, end, vec3(0)});
 * }
 *
 * // Upload and render (called once per frame)
 * void staticRender(const CameraBase& camera, const Window& window, IRenderer& renderer) {
 *     if (s_lines.empty()) return;
 *
 *     // Upload to GPU (single glBufferData)
 *     glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
 *     glBufferData(GL_ARRAY_BUFFER, s_lines.size() * sizeof(DebugLine),
 *                  s_lines.data(), GL_DYNAMIC_DRAW);
 *
 *     // Setup shader
 *     s_shader->bind();
 *     s_shader->setUniform("u_ViewProj", camera.getViewProjectionMatrix(aspect));
 *
 *     // Configure rendering
 *     glLineWidth(s_lineWidth);
 *     if (!s_depthTestEnabled) {
 *         glDisable(GL_DEPTH_TEST);  // X-ray mode
 *     }
 *
 *     // Draw all lines (single call)
 *     glBindVertexArray(s_vao);
 *     glDrawArrays(GL_LINES, 0, s_lines.size() * 2);
 *
 *     // Restore state
 *     glEnable(GL_DEPTH_TEST);
 *     glLineWidth(1.0f);
 * }
 *
 * // Clear (called after render)
 * void staticClear() {
 *     s_lines.clear();  // Remove all lines, ready for next frame
 * }
 * ```
 *
 * PERFORMANCE ANALYSIS:
 *
 * Cost per primitive:
 * - Line: 1 line (48 bytes)
 * - Sphere: ~50 lines (2.4 KB, 16 segments × 3 circles)
 * - Box: 12 lines (576 bytes, 12 edges)
 * - OBB: 12 lines (576 bytes)
 * - Frustum: 12 lines (576 bytes, pyramid edges)
 *
 * Typical scene (500 primitives):
 * - Memory: 500 × 48 bytes = 24 KB (negligible)
 * - Upload: One glBufferData (24 KB, ~0.01ms)
 * - Draw: One glDrawArrays (~0.05ms)
 * - Total overhead: <0.1ms per frame (imperceptible)
 *
 * Heavy scene (5000 primitives):
 * - Memory: 5000 × 48 bytes = 240 KB
 * - Upload: ~0.1ms
 * - Draw: ~0.5ms
 * - Total: ~0.6ms (acceptable, but consider reducing)
 *
 * BOUNDING VOLUME COMPARISON:
 *
 * AABB (drawBox - Yellow):
 * - Always aligned with world axes (X, Y, Z)
 * - Grows/shrinks as object rotates (contains rotated shape)
 * - Fast intersection tests (simple min/max checks)
 * - Good for: Broad-phase culling, spatial partitioning
 * - Tightness: Loosest for rotated objects
 *
 * OBB (drawOBB - Magenta):
 * - Rotates with object, maintains tight fit
 * - Same size regardless of rotation
 * - Tighter than AABB for rotated objects
 * - More expensive intersection tests
 * - Good for: Precise collision detection, rotated objects
 * - Tightness: Tightest box fit
 *
 * Sphere (drawSphere - Cyan):
 * - Rotation-invariant (looks same from all angles)
 * - Simplest intersection tests (distance check)
 * - Loosest fit (wastes space in corners)
 * - Good for: Fast frustum culling, rough collisions
 * - Tightness: Loosest overall fit
 *
 * COORDINATE SYSTEM:
 * - World space coordinates (same as scene objects)
 * - RGB axes convention: X=red (right), Y=green (up), Z=blue (forward)
 * - Grid on XZ plane (horizontal ground)
 *
 * CURRENT STATE (November 6, 2025):
 * - Immediate-mode API (draw functions + render)
 * - Static backend (global access)
 * - Line batching (single draw call per frame)
 * - Basic shapes (line, sphere, box, arrow, axes, grid)
 * - Advanced shapes (OBB, frustum - Week 4)
 * - Optional depth test (X-ray mode)
 * - Configurable line width
 * - One-frame lifetime (auto-clear)
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Lines Only:
 * - No filled shapes (solid boxes, spheres)
 * - No text rendering (can't label objects)
 * - Future: Filled shapes (Week 5+), text (Week 6+)
 *
 * 2. Single Line Width:
 * - All lines same width (no per-line control)
 * - Future: Per-line width (Week 5+)
 *
 * 3. Solid Lines Only:
 * - No dashed, dotted, or patterned lines
 * - Future: Line styles (Week 5+)
 *
 * 4. One-Frame Lifetime:
 * - All geometry cleared after render
 * - No persistent debug objects
 * - Future: Duration-based drawing (drawLine with lifetime, Week 5+)
 *
 * 5. No Depth Sorting:
 * - Draw order = call order
 * - No proper alpha blending
 * - Acceptable: Debug visualization, not production rendering
 *
 * INTEGRATION WITH ROADMAP:
 *
 * November 6, 2025: Initial implementation
 * - Immediate-mode debug renderer (static backend)
 * - Line batching (efficient rendering)
 * - Basic shapes (line, sphere, box, arrow, axes, grid)
 * - IDebugRenderer interface implementation
 *
 * (OBB + Frustum - November 2025):
 * - drawOBB() for oriented bounding boxes
 * - drawFrustum() for camera visualization
 * - Enhanced documentation (multi-camera, bounding volume comparison)
 *
 * (Enhancements):
 * - Text rendering (3D labels, statistics)
 * - Duration-based drawing (lifetime parameter)
 * - Per-primitive depth mode
 * - Line styles (dashed, dotted)
 * - Per-line width control
 * - Filled shapes (solid boxes, spheres)
 * - 2D overlay primitives (HUD debugging)
 * - Time: 2-3 weeks total
 *
 * DEPENDENCIES:
 * - renderer/interface/IDebugRenderer.h: Abstract interface
 * - renderer/interface/IRenderer.h: Renderer state management
 * - renderer/opengl/GLShader.h: Shader implementation
 * - <glad/glad.h>: OpenGL function loader
 * - <vector>: Line storage
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Static storage, OpenGL context requirement
 * - All operations on main render thread only
 * - draw*() calls: Accumulate in s_lines vector (not thread-safe)
 * - render(): Upload and draw (OpenGL calls, main thread only)
 *
 * REFERENCES:
 * - Immediate-mode GUIs: Inspiration for API design
 * - Dear ImGui: Immediate-mode pattern reference
 * - IDebugRenderer.h: Interface documentation
 *
 * HISTORY:
 *  October 9, 2025: Original implementation
 * - Debugrenderer was hardcoded OpenGL class with minimal debug visual features
 * - Added debugging as necessary, I don't like investing to much time in visual stuff I don't think is worth it
 * - Performance also was tied to transform, since the matrices previously weren't cached so performance was worse,
 * - with imgui rendering as AABB and Bounding sphere kept having to recalculate the matrix
 * 
 * November 6, 2025: Initial implementation
 * - Immediate-mode debug renderer (static backend)
 * - Line batching (upload once, draw once per frame)
 * - Basic shapes (line, sphere, box, arrow, axes, grid)
 * - Optional depth test (X-ray mode)
 * - Configurable line width
 * - IDebugRenderer interface implementation
 *
 * (November 2025): Advanced shapes
 * - drawOBB() for oriented bounding boxes
 * - drawFrustum() for camera view frustum
 * - Enhanced documentation (multi-camera examples)
 * - Bounding volume comparison guide
 *
 */

namespace Engine
{
    class GLDebugRenderer : public IDebugRenderer
    {
    public:
        // IDebugRenderer interface implementation (forwards to static)
        void init() override { staticInit(); }
        void shutdown() override { staticShutdown(); }

        void drawLine(const vec3& start, const vec3& end, const vec3& color) override
        {
            staticDrawLine(start, end, color);
        }

        void drawSphere(const vec3& center, float radius, const vec3& color, int segments = 16) override
        {
            staticDrawSphere(center, radius, color, segments);
        }

        void drawBox(const vec3& center, const vec3& halfSize, const vec3& color) override
        {
            staticDrawBox(center, halfSize, color);
        }

        void drawOBB(const vec3& center, const vec3& halfSize, const mat4& rotation, const vec3& color) override
        {
            staticDrawOBB(center, halfSize, rotation, color);
        }

        void drawFrustum(const mat4& viewProj, const vec3& color) override
        {
            staticDrawFrustum(viewProj, color);
        }

        void drawArrow(const vec3& start, const vec3& end, const vec3& color, float tipSize = 0.2f) override
        {
            staticDrawArrow(start, end, color, tipSize);
        }

        void drawAxes(const vec3& position, float size = 1.0f) override
        {
            staticDrawAxes(position, size);
        }

        void drawAxes(const Transform& transform, float size = 1.0f) override
        {
            staticDrawAxesTransform(transform, size);
        }

        void drawGrid(float size, int divisions, const vec3& color = vec3(0.5f)) override
        {
            staticDrawGrid(size, divisions, color);
        }

        // FIXED: Non-template render using CameraBase
        void render(const CameraBase& camera, const Window& window, IRenderer& renderer) override
        {
            staticRender(camera, window, renderer);
        }

        void clear() override { staticClear(); }
        void setDepthTest(bool enabled) override { staticSetDepthTest(enabled); }
        void setLineWidth(float width) override { staticSetLineWidth(width); }
        bool getDepthTest() const override { return s_depthTestEnabled; }
        float getLineWidth() const override { return s_lineWidth; }

        // Static implementations (actual work happens here)
        static void staticInit();
        static void staticShutdown();
        static void staticDrawLine(const vec3& start, const vec3& end, const vec3& color);
        static void staticDrawSphere(const vec3& center, float radius, const vec3& color, int segments = 16);
        static void staticDrawBox(const vec3& center, const vec3& halfSize, const vec3& color);
        static void staticDrawOBB(const vec3& center, const vec3& halfSize, const mat4& rotation, const vec3& color);
        static void staticDrawFrustum(const mat4& viewProj, const vec3& color);
        static void staticDrawArrow(const vec3& start, const vec3& end, const vec3& color, float tipSize = 0.2f);
        static void staticDrawAxes(const vec3& position, float size = 1.0f);
        static void staticDrawAxesTransform(const Transform& transform, float size = 1.0f);
        static void staticDrawGrid(float size, int divisions, const vec3& color);
        static void staticClear();
        static void staticSetLineWidth(float width);
        static void staticSetDepthTest(bool enabled);

        // FIXED: Non-template static render using CameraBase
        static void staticRender(const CameraBase& camera, const Window& window, IRenderer& renderer);

        // Static getters for UI
        static bool staticGetDepthTest() { return s_depthTestEnabled; }
        static float staticGetLineWidth() { return s_lineWidth; }

    private:
        struct DebugLine
        {
            vec3 start;
            vec3 color;
            vec3 end;
            vec3 padding;
        };

        static std::vector<DebugLine> s_lines;
        static std::unique_ptr<GLShader> s_shader;
        static GLuint s_vao, s_vbo;
        static bool s_initialized;
        static bool s_depthTestEnabled;
        static float s_lineWidth;

        static void ensureInitialized();
        static void uploadLinesToGPU();
    };
}