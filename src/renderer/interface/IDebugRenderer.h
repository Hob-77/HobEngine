#pragma once
#include "math/EngineMath.h"

/*
 * IDebugRenderer.h
 *
 * PURPOSE:
 * API-agnostic debug rendering abstraction for wireframe visualization during development.
 * Renders bounding volumes, frustums, coordinate axes, grids, and arbitrary lines. Essential
 * development tool for debugging culling, physics, AI, and rendering. NOT for production builds
 * (compile out with #ifdef DEBUG or #ifndef NDEBUG).
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Debug visualization (wireframes, bounding boxes, frustums) requires rendering lines,
 * which is heavily API-specific (OpenGL GL_LINES vs Vulkan VK_PRIMITIVE_TOPOLOGY_LINE_LIST).
 * Direct use of glDrawArrays(GL_LINES) makes debug code non-portable.
 *
 * Solution: Interface abstraction for immediate-mode debug rendering.
 * - Application code uses IDebugRenderer* (doesn't know if OpenGL or Vulkan)
 * - GLDebugRenderer implements with OpenGL GL_LINES and dynamic VAO/VBO
 * - VKDebugRenderer implements with Vulkan line list topology and dynamic buffers
 * - Switching APIs = zero changes to Scene culling visualization or debug tools
 *
 * Key Insight: Debug rendering is inherently immediate-mode (draw lines on demand), but modern
 * GPUs are batched. Interface provides immediate-mode API (drawLine, drawSphere) while batching
 * internally for performance (accumulate all lines, single draw call per frame).
 *
 * CRITICAL DESIGN LIMITATION - Static Implementation (KNOWN ISSUE):
 *
 * Problem: GLDebugRenderer currently uses static methods and static data members for
 * convenience during initial development. This was a pragmatic choice to get debug
 * visualization working quickly, but has significant limitations:
 *
 * Issues with Static Design:
 * 1. Single Global Context: Can't have multiple debug renderers (split-screen, editor + game)
 * 2. No Multithreading: Static state = global mutex or race conditions
 * 3. Not Scalable: Can't debug multiple scenes or viewports simultaneously
 * 4. Vulkan Incompatible: Vulkan requires explicit contexts, no global state
 * 5. Testing Difficult: Can't mock or isolate debug renderer for unit tests
 *
 * Why This Exists:
 * - pragmatism: Needed debug visualization working immediately for frustum culling
 * - Single viewport: OpenGL development uses one context, static "works for now"
 * - Rapid iteration: Static methods simpler than passing IDebugRenderer* everywhere
 * - Temporary solution: Accepted technical debt with plan to refactor
 *
 * Refactor Plan (Pre-Vulkan):
 * ```cpp
 * // Current (static):
 * GLDebugRenderer::drawSphere(center, radius, color);  // Static call
 *
 * // Future (instance-based):
 * IDebugRenderer* debug = renderDevice->createDebugRenderer();
 * debug->drawSphere(center, radius, color);  // Instance call
 * ```
 *
 * Required Changes:
 * 1. Remove all static members from GLDebugRenderer (line buffer, VAO, VBO, shader)
 * 2. Store line data as instance members (std::vector<DebugLine> m_lines)
 * 3. Pass IDebugRenderer* through Application -> Scene -> rendering code
 * 4. Create one debug renderer per viewport/context
 * 5. Enable multiple simultaneous debug renderers for split-screen/editor
 *
 * Time to Refactor: 3-4 hours (straightforward refactor, touch ~10 files)
 * When: Before Vulkan implementation (Vulkan cannot use static global state)
 * Priority: Medium (works for single-viewport OpenGL, blocks Vulkan)
 *
 * Current Acceptable Because:
 * - Single viewport development (no split-screen yet)
 * - OpenGL-only (Vulkan not implemented)
 * - Debug tool (not production code, no shipping concerns)
 * - Fast iteration (static methods convenient during prototyping)
 *
 * This follows "ship beats perfect" - get debug visualization working now, refactor before
 * Vulkan. The interface is already correct (instance-based API), only implementation needs fixing.
 *
 * DESIGN PHILOSOPHY:
 * - Pure virtual interface: No OpenGL/Vulkan code in this header
 * - Immediate-mode API: Draw commands accumulate, render() flushes batch
 * - Line-based rendering: All shapes decomposed to lines (GL_LINES)
 * - Development tool: Not for production (compile out with #ifndef NDEBUG)
 * - Visual clarity: Bright colors (red, green, blue) for easy identification
 * - X-ray mode: Optional depth test disable for seeing through geometry
 *
 * KEY CONCEPTS:
 * 1. Immediate-Mode Rendering: Draw commands issued immediately, batched internally
 *    - API: drawLine(start, end, color) - called many times per frame
 *    - Implementation: Accumulates lines in buffer, renders all in single draw call
 *    - Trade-off: Simple API vs batched performance
 *
 * 2. Batched Line Rendering: Collect all lines, draw once
 *    - Without batching: 1000 lines = 1000 draw calls (slow, ~10ms)
 *    - With batching: 1000 lines = 1 draw call (fast, ~0.1ms)
 *    - Implementation: std::vector<DebugLine> accumulates, render() uploads to GPU
 *
 * 3. Dynamic Buffers: Line buffer updated every frame
 *    - OpenGL: glBufferData with GL_DYNAMIC_DRAW hint
 *    - Vulkan: VkBuffer with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, map/unmap
 *    - Cost: Acceptable for debug (not hot path, debug-only builds)
 *
 * 4. Depth Testing Control: See through vs occluded
 *    - Depth test ON: Lines occluded by geometry (realistic visualization)
 *    - Depth test OFF: Lines always visible (x-ray mode, see through walls)
 *    - Use case: X-ray mode useful for seeing culled objects behind walls
 *
 * 5. Line Width: Thickness control (platform-dependent)
 *    - OpenGL: glLineWidth() always works (hardware support)
 *    - Vulkan: Requires VK_EXT_line_rasterization extension (not guaranteed)
 *    - Default: 1.0f (single pixel), can increase for visibility
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // Create debug renderer (one per viewport)
 * auto debug = renderDevice->createDebugRenderer();
 * debug->init();  // Initialize GPU resources (VAO, VBO, shader)
 *
 * // Configure settings
 * debug->setDepthTest(false);  // X-ray mode (see through geometry)
 * debug->setLineWidth(2.0f);   // Thicker lines for visibility
 *
 * // === UPDATE LOOP ===
 * void update() {
 *     debug->clear();  // Clear previous frame's accumulated lines
 *
 *     // Draw coordinate axes at origin (X=red, Y=green, Z=blue)
 *     debug->drawAxes(vec3(0, 0, 0), 5.0f);
 *
 *     // Draw ground grid
 *     debug->drawGrid(100.0f, 20, vec3(0.5f, 0.5f, 0.5f));
 *
 *     // Visualize bounding volumes
 *     for (auto& object : scene->getObjects()) {
 *         auto sphere = object->mesh->boundingSphere.toWorld(object->transform);
 *         debug->drawSphere(sphere.center, sphere.radius, vec3(0, 1, 0));
 *
 *         auto aabb = object->mesh->aabb.toWorld(object->transform);
 *         debug->drawBox(aabb.getCenter(), aabb.getExtents(), vec3(1, 1, 0));
 *     }
 *
 *     // Visualize frustum culling (red = culled, green = visible)
 *     for (auto& object : scene->getObjects()) {
 *         auto sphere = object->mesh->boundingSphere.toWorld(object->transform);
 *         bool visible = camera.getFrustum().intersects(sphere);
 *         vec3 color = visible ? vec3(0, 1, 0) : vec3(1, 0, 0);
 *         debug->drawSphere(sphere.center, sphere.radius, color);
 *     }
 *
 *     // Draw camera frustum
 *     mat4 viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
 *     debug->drawFrustum(viewProj, vec3(1, 0, 1));
 *
 *     // Draw custom lines
 *     debug->drawLine(vec3(0, 0, 0), vec3(10, 0, 0), vec3(1, 0, 0));  // Red line
 *     debug->drawArrow(lightPos, lightPos + lightDir * 5.0f, vec3(1, 1, 0), 0.5f);
 * }
 *
 * // === RENDER LOOP ===
 * void render() {
 *     // Render scene normally
 *     scene->render(camera, shader);
 *
 *     // Render all accumulated debug lines in single draw call
 *     debug->render(camera, window, renderer);
 * }
 *
 * // === CLEANUP ===
 * debug->shutdown();  // Free GPU resources
 * ```
 *
 * INTEGRATION WITH ENGINE:
 * Before Refactor:
 * ```cpp
 * // No debug visualization - blind debugging
 * // Had to use printf debugging or guess why objects weren't rendering
 * ```
 *
 * After Refactor:
 * ```cpp
 * // API-agnostic debug rendering
 * auto debug = renderDevice->createDebugRenderer();
 * debug->drawSphere(position, radius, vec3(1, 0, 0));  // Visual debugging
 * ```
 *
 * TYPICAL INTEGRATION PATTERN:
 * Scene class uses debug renderer for culling visualization:
 * ```cpp
 * class Scene {
 *     bool m_showDebug = false;
 *
 *     void render(CameraBase& camera, IShader& shader, IDebugRenderer& debug) {
 *         // Normal rendering with frustum culling
 *         const Frustum& frustum = camera.getFrustum();
 *
 *         for (auto& object : m_objects) {
 *             auto sphere = object.mesh->boundingSphere.toWorld(object.transform);
 *
 *             if (!frustum.intersects(sphere)) {
 *                 // Culled - visualize in red if debug enabled
 *                 if (m_showDebug) {
 *                     debug.drawSphere(sphere.center, sphere.radius, vec3(1, 0, 0));
 *                 }
 *                 continue;
 *             }
 *
 *             // Visible - render normally, visualize in green if debug enabled
 *             if (m_showDebug) {
 *                 debug.drawSphere(sphere.center, sphere.radius, vec3(0, 1, 0));
 *             }
 *
 *             object.mesh->draw();
 *         }
 *
 *         // Draw frustum bounds
 *         if (m_showDebug) {
 *             mat4 viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
 *             debug.drawFrustum(viewProj, vec3(0, 1, 1));
 *         }
 *     }
 * };
 * ```
 *
 * DRAW COMMAND IMPLEMENTATIONS:
 * Each high-level shape decomposed to lines:
 *
 * drawSphere(center, radius, color, segments):
 * - Creates 3 orthogonal circles (XY, XZ, YZ planes)
 * - Each circle: segments line segments
 * - Total: segments x 3 lines
 * - Default 16 segments = 48 lines per sphere
 *
 * drawBox(center, halfSize, color):
 * - Creates 12 lines for cube edges (4 bottom + 4 top + 4 vertical)
 * - Oriented axis-aligned bounding box (AABB)
 * - Total: 12 lines per box
 *
 * drawOBB(center, halfSize, rotation, color):
 * - Oriented bounding box (OBB) with rotation matrix
 * - Same as drawBox but applies rotation to vertices
 * - Total: 12 lines per OBB
 *
 * drawFrustum(viewProj, color):
 * - Extracts 8 frustum corner points from view-projection matrix
 * - Draws 12 lines connecting corners (4 near + 4 far + 4 connecting)
 * - Total: 12 lines per frustum
 *
 * drawArrow(start, end, color, tipSize):
 * - Main line from start to end
 * - Two tip lines forming arrow head
 * - Total: 3 lines per arrow
 *
 * drawAxes(position, size):
 * - X axis: Red line (+X direction)
 * - Y axis: Green line (+Y direction)
 * - Z axis: Blue line (+Z direction)
 * - Total: 3 lines per axes
 *
 * drawGrid(size, divisions, color):
 * - Horizontal lines (X direction): divisions + 1 lines
 * - Vertical lines (Z direction): divisions + 1 lines
 * - Example: 100.0f size, 20 divisions = 42 lines (21 x 2)
 * - Total: (divisions + 1) x 2 lines
 *
 * COLOR CONVENTIONS:
 * Standard debug color meanings:
 * - Red (1, 0, 0): Error, culled, disabled, negative
 * - Green (0, 1, 0): Success, visible, enabled, positive
 * - Blue (0, 0, 1): Information, depth, Z-axis
 * - Yellow (1, 1, 0): Warning, selected, highlighted
 * - Cyan (0, 1, 1): Camera, frustum, view-related
 * - Magenta (1, 0, 1): Light, special, attention
 * - White (1, 1, 1): Neutral, grid, generic
 * - Gray (0.5, 0.5, 0.5): Background, grid, subtle
 *
 * Coordinate axes (universal standard):
 * - X axis: Red (mnemonic: X marks the spot)
 * - Y axis: Green (mnemonic: grass grows up)
 * - Z axis: Blue (mnemonic: sky is blue)
 *
 * PERFORMANCE:
 * Typical Frame Debug Load:
 * - 100 objects with spheres: 100 x 48 lines = 4,800 lines
 * - 100 objects with AABBs: 100 x 12 lines = 1,200 lines
 * - Grid (20 divisions): 42 lines
 * - Frustum: 12 lines
 * - Axes: 3 lines
 * - Total: ~6,000 lines per frame
 *
 * Rendering Cost (November 17, 2025):
 * - 6,000 lines in single draw call: ~0.2-0.5ms per frame
 * - Dynamic buffer update: ~0.1ms (upload 6000 x 24 bytes = 144 KB)
 * - Total overhead: ~0.3-0.6ms (acceptable for debug builds)
 * - Context: 16.67ms budget at 60 FPS, debug uses <4%
 *
 * Memory Usage:
 * - Per line: 2 vertices x 12 bytes (vec3) + 2 colors x 12 bytes = 48 bytes
 * - 6,000 lines: 288 KB GPU memory (dynamic buffer)
 * - Negligible compared to scene geometry (10-100 MB typical)
 *
 * Scalability:
 * - 10,000 lines: ~0.5-1.0ms (still acceptable)
 * - 100,000 lines: ~5-10ms (noticeable, but debug-only)
 * - Mitigation: Compile out debug rendering in release builds (#ifndef NDEBUG)
 *
 * DEPTH TEST MODES:
 * Depth Test Enabled (default):
 * - Lines occluded by geometry (realistic visualization)
 * - Use case: See bounding volumes in correct spatial relationship
 * - Visual: Lines hidden behind objects, looks correct
 *
 * Depth Test Disabled (x-ray mode):
 * - Lines always visible, render over everything
 * - Use case: Debug culled objects behind walls, see through scene
 * - Visual: All lines visible regardless of depth, see inside geometry
 *
 * Toggle at runtime:
 * ```cpp
 * if (Input::isKeyPressed(KEY_F1)) {
 *     bool xray = debug->getDepthTest();
 *     debug->setDepthTest(!xray);  // Toggle x-ray mode
 * }
 * ```
 *
 * LINE WIDTH:
 * Platform differences:
 * - OpenGL: glLineWidth(width) always works (driver support)
 * - Vulkan: Requires VK_EXT_line_rasterization extension
 *   - Not guaranteed on all hardware
 *   - Fall back to 1.0f width if extension unavailable
 *
 * Typical values:
 * - 1.0f: Single pixel (default, always works)
 * - 2.0f: Thicker, more visible (good for large scenes)
 * - 3.0f+: Very thick, can obscure geometry
 *
 * Usage:
 * ```cpp
 * debug->setLineWidth(2.0f);  // Thicker lines for better visibility
 * ```
 *
 * CURRENT STATE (November 2025):
 * Implemented features:
 * - Wireframe sphere rendering (48 lines per sphere, 16 segments)
 * - AABB visualization (12 lines per box)
 * - Frustum visualization (12 lines, 8 corners)
 * - Grid rendering (configurable size and divisions)
 * - Coordinate axes (X=red, Y=green, Z=blue)
 * - Line rendering with color
 * - Depth test toggle (x-ray mode)
 *
 * Integration:
 * - Used by Scene for frustum culling visualization
 * - Shows culled objects (red) vs visible objects (green)
 * - Validates frustum culling correctness
 *
 * Known Limitation:
 * - Static implementation (see CRITICAL DESIGN LIMITATION section)
 * - Single debug context (works for current single-viewport development)
 *
 * FUTURE USE CASES:
 * (Lighting):
 * - Draw light volumes (sphere radius = attenuation distance)
 * - Draw light direction arrows (directional lights)
 * - Visualize light influence on objects
 *
 * (Animation):
 * - Draw skeleton bones (lines connecting joints)
 * - Draw joint coordinate axes (visualize rotations)
 * - Visualize bone weights (colored vertices)
 *
 * Physics System (Future):
 * - Draw collision shapes (boxes, spheres, capsules)
 * - Draw contact points (red dots at collision points)
 * - Draw velocity vectors (arrows showing movement)
 * - Draw forces (arrows showing applied forces)
 *
 * AI/Navigation (Future):
 * - Draw pathfinding routes (lines connecting waypoints)
 * - Draw nav mesh (triangle edges)
 * - Draw agent perception radius (circles around characters)
 *
 * Profiler/Tools (Future):
 * - Draw frame time graph (line plot over time)
 * - Draw memory usage graph
 * - Draw performance hotspots (colored regions)
 *
 * COMPILE-OUT STRATEGY:
 * Debug rendering should be removed in release builds:
 *
 * ```cpp
 * // Header (IDebugRenderer.h)
 * #ifndef NDEBUG
 *     class IDebugRenderer { };
 *#else
 *     class IDebugRenderer {
 *         // Stub implementation - all methods are no-ops
 *         void drawLine(...) {}
 *         void drawSphere(...) {}
 *         // Compiler optimizes away (inline + empty body = zero cost)
 *     };
 * #endif
 * 
 * // Usage (Scene.cpp)
 * void Scene::render(..., IDebugRenderer& debug) {
 *     // In release: debug calls compiled away (zero cost)
 *     debug.drawSphere(center, radius, color);
 * }
 * ```
 * 
 * Result:
 * - Debug builds: Full debug rendering with ~0.5ms cost
 * - Release builds: Zero cost (calls optimized away by compiler)
 * 
 * IMPLEMENTATIONS:
 * - GLDebugRenderer (November 2025): OpenGL GL_LINES implementation
 *   - Dynamic VAO/VBO for line buffer
 *   - Simple vertex shader (MVP transform only)
 *   - Fragment shader (per-vertex color)
 *   - Batched rendering (accumulate lines, single glDrawArrays call)
 *   - KNOWN ISSUE: Static implementation (see CRITICAL DESIGN LIMITATION)
 *   - Status: Functional for single-viewport development, needs refactor pre-Vulkan
 * 
 * - VKDebugRenderer (Future): Vulkan line list implementation
 *   - Dynamic VkBuffer for line vertices
 *   - VK_PRIMITIVE_TOPOLOGY_LINE_LIST topology
 *   - Requires instance-based design (no static state in Vulkan)
 *   - VK_EXT_line_rasterization for wide lines (optional, fallback to 1.0f)
 *   - Status: Planned, requires GLDebugRenderer refactor first
 *   - Estimate: 3-4 days (2-3 days Vulkan impl + 3-4 hours refactor GLDebugRenderer)
 * 
 * DEPENDENCIES:
 * - math/EngineMath.h: GLM wrapper (vec3, mat4 types)
 * - CameraBase.h: Forward declared for polymorphic camera support
 * - Window.h: Forward declared for viewport dimensions
 * - Transform.h: Forward declared for drawAxes(Transform) overload
 * - IRenderer.h: Forward declared for state management during debug rendering
 * 
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL context is thread-local
 * - Current static design: NOT thread-safe (global state)
 * - Future instance design: Still not thread-safe (OpenGL context limitation)
 * - Vulkan: Could be thread-safe with proper synchronization (secondary command buffers)
 * - Current: All debug rendering on main render thread only
 * 
 * REFERENCES:
 * - The Cherno C++ Series: "Interfaces in C++" (foundational design pattern)
 * - Gang of Four Design Patterns: Abstract Factory (debug renderer creation)
 * - Learn OpenGL (learnopengl.com): Line rendering and dynamic buffers
 * - Real-Time Rendering 4th Ed., Chapter 18: Debugging and visualization techniques
 * - Game Engine Architecture 3rd Ed., Chapter 14: Debug visualization systems
 * - GDC talks: "Debugging Techniques for Rendering Engines" (various years)
 * - Dear ImGui: Immediate-mode GUI paradigm (inspiration for API design)
 * 
 * FUTURE ENHANCEMENTS:
 * Pre-Vulkan (Priority: High):
 * - Refactor GLDebugRenderer to instance-based design (remove static members)
 * - Pass IDebugRenderer* through Application -> Scene -> systems
 * - Enable multiple debug renderers (split-screen, editor + game view)
 * - Time: 3-4 hours
 * - Blocker: Must complete before Vulkan implementation
 * 
 * (Lighting Debug):
 * - Add drawCone() for spotlight visualization
 * - Add drawCircle() for point light attenuation radius
 * - Add drawDirectionalLight() (arrows showing light direction)
 * - Time: 1-2 hours
 * 
 * (Animation Debug):
 * - Add drawSkeleton() for bone hierarchies
 * - Add drawJoint() with coordinate axes
 * - Add drawBoneWeights() for vertex skinning visualization
 * - Time: 2-3 hours
 * 
 * (Vulkan):
 * - VKDebugRenderer implementation with dynamic VkBuffer
 * - Line list topology (VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
 * - Wide lines extension (VK_EXT_line_rasterization, optional)
 * - Secondary command buffers for potential threading
 * - Time: 3-4 days (includes GLDebugRenderer refactor)
 * 
 * Optional (Quality of Life):
 * - Text rendering (draw labels, numbers, performance stats)
 * - Filled shapes (translucent planes, solid spheres for volumes)
 * - Line stipple patterns (dashed lines, dotted lines)
 * - Billboard sprites (icons for lights, cameras, markers)
 * - Recording/playback (save debug frames, replay for analysis)
 * 
 * HISTORY:
 * November 6, 2025: Initial creation during interface refactor
 * - Created pure virtual interface with immediate-mode API
 * - Designed batched rendering (accumulate + flush pattern)
 * - Added depth test toggle for x-ray mode
 * - Implemented by GLDebugRenderer (OpenGL GL_LINES, dynamic VAO/VBO)
 * - PRAGMATIC CHOICE: Used static implementation for rapid development
 *   - Accepted technical debt: Must refactor to instance-based before Vulkan
 *   - Rationale: Single viewport OpenGL development, works for current needs
 *   - Timeline: Refactor pre-Vulkan (3-4 hours, straightforward)
 * 
 * November 7-8, 2025: Integration and validation
 * - Used by Scene for frustum culling visualization
 * - Validated bounding sphere rendering (48 lines per sphere)
 * - Validated AABB rendering (12 lines per box)
 * - Tested with 100-object scene (6,000+ lines, ~0.5ms overhead)
 * - Enabled rapid debugging of culling system (saw red/green spheres immediately)
 * - Functional for development, needs refactor before Vulkan
 * 
 */

 // Forward declarations
namespace Engine
{
    class CameraBase;  // Common base for all camera types
    class Window;
    class Transform;
    class IRenderer;
}

namespace Engine
{
    class IDebugRenderer
    {
    public:
        virtual ~IDebugRenderer() = default;

        // Initialization (call once after creation)
        virtual void init() = 0;
        virtual void shutdown() = 0;

        // Drawing commands (immediate-mode style, batched internally)
        // Color: RGB in [0,1] range
        virtual void drawLine(const vec3& start, const vec3& end, const vec3& color) = 0;
        virtual void drawSphere(const vec3& center, float radius, const vec3& color, int segments = 16) = 0;
        virtual void drawBox(const vec3& center, const vec3& halfSize, const vec3& color) = 0;
        virtual void drawOBB(const vec3& center, const vec3& halfSize, const mat4& rotation, const vec3& color) = 0;
        virtual void drawFrustum(const mat4& viewProj, const vec3& color) = 0;
        virtual void drawArrow(const vec3& start, const vec3& end, const vec3& color, float tipSize = 0.2f) = 0;

        // Coordinate axes (X=red, Y=green, Z=blue)
        virtual void drawAxes(const vec3& position, float size = 1.0f) = 0;
        virtual void drawAxes(const Transform& transform, float size = 1.0f) = 0;

        // Grid (useful for editor ground plane)
        virtual void drawGrid(float size, int divisions, const vec3& color = vec3(0.5f)) = 0;

        // Rendering (flushes all batched lines in one draw call)
        // Uses CameraBase to support all camera types polymorphically
        virtual void render(const CameraBase& camera, const Window& window, IRenderer& renderer) = 0;

        // Frame management
        virtual void clear() = 0;  // Call at start of frame to clear previous lines

        // Settings
        virtual void setDepthTest(bool enabled) = 0;  // false = xray mode
        virtual void setLineWidth(float width) = 0;   // OpenGL: always works, Vulkan: requires extension
        virtual bool getDepthTest() const = 0;
        virtual float getLineWidth() const = 0;
    };
}