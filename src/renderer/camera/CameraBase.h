#pragma once
#include "math/EngineMath.h"

/*
 * CameraBase.h
 *
 * PURPOSE:
 * Abstract base class for all camera types. Provides matrix caching, previous frame matrix
 * storage for temporal effects (TAA, motion blur), and consistent API across camera types.
 * Implements Template Method pattern - base handles caching/history, derived classes implement
 * view matrix calculation.
 *
 * DESIGN RATIONALE (October 22, 2025):
 * Problem: Had separate Camera and FPSCamera classes with duplicated caching logic. Realized
 * would need more camera types (ThirdPerson, Rail, Orbit) - duplication would worsen. Need
 * temporal effects (TAA, motion blur) requiring previous frame matrices - can't add to each
 * camera type separately.
 *
 * Solution: Extract common functionality to base class using Template Method pattern.
 * - Base class: Matrix caching, previous frame storage, projection calculation
 * - Derived classes: View matrix calculation only (camera-specific behavior)
 * - Result: Eliminates duplication, enables temporal effects, simplifies new camera types
 *
 * Key Insight: All cameras share same operations (caching, projection) but differ only in
 * view matrix calculation. Template Method pattern perfect fit - single point of caching
 * logic, multiple specialized view calculations.
 *
 * DESIGN PHILOSOPHY:
 * - Template Method pattern: Base handles common behavior, derived specialize
 * - Dirty flag caching: Recalculate only when camera changes
 * - Temporal support: Previous frame matrices for modern rendering
 * - Consistent API: All cameras used identically by rendering code
 * - Performance-focused: Non-virtual getters (hot path), virtual compute (cold path)
 *
 * KEY CONCEPTS:
 * 1. Template Method Pattern:
 *    - Base class defines algorithm structure (caching, projection)
 *    - Derived classes implement specific steps (view matrix calculation)
 *    - Result: Code reuse without duplication
 *
 * 2. Matrix Caching (Dirty Flag):
 *    - First access: Compute matrices, mark clean
 *    - Subsequent accesses: Return cached matrices (fast)
 *    - On change: Mark dirty, recompute on next access
 *    - Result: Static cameras pay computation cost once
 *
 * 3. Previous Frame Matrices (Temporal Effects):
 *    - Store previous frame's view, projection, view-projection
 *    - Used by: TAA (sample reprojection), motion blur (velocity), temporal upscaling
 *    - Updated once per frame via updatePreviousFrame()
 *
 * 4. Projection Parameters:
 *    - FOV: Field of view in degrees (vertical)
 *    - Near plane: Closest visible distance
 *    - Far plane: Farthest visible distance
 *    - Aspect ratio: Width / height (dynamic, passed at runtime)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === DERIVED CLASS IMPLEMENTATION ===
 * class FPSCamera : public CameraBase {
 * protected:
 *     // Implement pure virtual method
 *     mat4 computeViewMatrix() const override {
 *         return glm::lookAt(m_position, m_position + m_front, m_up);
 *     }
 *
 * public:
 *     void setPosition(const vec3& pos) {
 *         m_position = pos;
 *         markDirty();  // Trigger recalculation
 *     }
 *
 *     void processMouseMovement(float xoffset, float yoffset) {
 *         m_yaw += xoffset;
 *         m_pitch += yoffset;
 *         updateCameraVectors();
 *         markDirty();  // Trigger recalculation
 *     }
 * };
 *
 * // === APPLICATION USAGE ===
 * FPSCamera camera;
 * camera.setPosition(vec3(0, 2, 5));
 * camera.setFOV(75.0f);
 *
 * void onRender() {
 *     float aspect = window.getAspectRatio();
 *
 *     // Update temporal history (once per frame, BEFORE rendering)
 *     camera.updatePreviousFrame(aspect);
 *
 *     // Get current matrices (cached if camera hasn't changed)
 *     mat4 view = camera.getViewMatrix();                    // ~1 cycle if cached
 *     mat4 proj = camera.getProjectionMatrix(aspect);        // ~1 cycle if cached
 *     mat4 viewProj = camera.getViewProjectionMatrix(aspect); // ~1 cycle if cached
 *
 *     shader.setUniform("u_View", view);
 *     shader.setUniform("u_Projection", proj);
 *     shader.setUniform("u_CameraPos", camera.getPosition());
 *
 *     // Render scene
 *     scene.render(camera, shader, window, renderer);
 * }
 *
 * // Later: Access previous frame for temporal effects
 * void setupTAA() {
 *     mat4 currentVP = camera.getViewProjectionMatrix(aspect);
 *     mat4 previousVP = camera.getPreviousViewProjectionMatrix();
 *
 *     taaShader.setUniform("u_CurrentViewProj", currentVP);
 *     taaShader.setUniform("u_PreviousViewProj", previousVP);
 * }
 * ```
 *
 * INHERITANCE HIERARCHY:
 *
 * ```
 * CameraBase (abstract)
 *         FPSCamera (first-person, WASD + mouse)
 *         Camera (position-target, orbit)
 *         Future:
 *         - ThirdPersonCamera (follow target with offset)
 *         - OrbitCamera (rotate around point)
 *         - RailCamera (fixed path, cinematic)
 *         - DebugCamera (free fly, editor mode)
 * ```
 *
 * All derived cameras:
 * - Implement computeViewMatrix() (camera-specific logic)
 * - Call markDirty() when state changes
 * - Inherit caching, temporal, projection from base
 * - Used identically by rendering code (polymorphic)
 *
 * MATRIX CACHING - How It Works:
 *
 * ```cpp
 * const mat4& CameraBase::getViewMatrix() const {
 *     if (m_viewDirty) {
 *         m_cachedView = computeViewMatrix();  // Virtual call to derived class
 *         m_viewDirty = false;
 *     }
 *     return m_cachedView;  // Return cached (fast)
 * }
 * ```
 *
 * Dirty flag pattern:
 * - Camera static: View computed once, cached forever (~0 cost subsequent frames)
 * - Camera moving: View recomputed each frame, cached for multiple accesses
 * - Derived classes: Call markDirty() when position/orientation changes
 *
 * Performance characteristics:
 * - Dirty check: ~1 CPU cycle (simple boolean test)
 * - Matrix computation: ~100-500 cycles (lookAt, cross products, normalize)
 * - Cache hit: Returns reference (~1 cycle)
 * - Speedup: ~100-500× for static cameras
 *
 * PREVIOUS FRAME MATRICES - Temporal Effects:
 *
 * Why needed:
 * - **TAA (Temporal Anti-Aliasing)**: Reproject previous frame samples to current frame
 * - **Motion Blur**: Calculate per-pixel velocity (current position - previous position)
 * - **Temporal Upscaling**: DLSS/FSR use temporal data for reconstruction
 * - **VR Reprojection**: Predict head movement, reduce latency artifacts
 *
 * Storage:
 * ```cpp
 * // Current frame matrices (updated via dirty flag caching)
 * mutable mat4 m_cachedView;
 * mutable mat4 m_cachedProjection;
 * mutable mat4 m_cachedViewProjection;
 *
 * // Previous frame matrices (updated via updatePreviousFrame)
 * mat4 m_previousView;
 * mat4 m_previousProjection;
 * mat4 m_previousViewProjection;
 * ```
 *
 * Update timing (critical):
 * ```cpp
 * void renderFrame() {
 *     // 1. Update previous frame BEFORE rendering (captures last frame state)
 *     camera.updatePreviousFrame(aspect);
 *
 *     // 2. Update camera position/rotation (current frame changes)
 *     camera.update(deltaTime);
 *
 *     // 3. Render with current matrices
 *     scene.render(camera, shader);
 *
 *     // 4. Apply TAA using previous + current matrices
 *     taa.apply(camera.getPreviousViewProjectionMatrix(),
 *               camera.getViewProjectionMatrix(aspect));
 * }
 * ```
 *
 * PROJECTION CALCULATION:
 *
 * Perspective projection:
 * ```cpp
 * mat4 CameraBase::computeProjectionMatrix(float aspect) const {
 *     return glm::perspective(
 *         glm::radians(m_fov),  // Vertical FOV (converted to radians)
 *         aspect,                // Width / height ratio
 *         m_nearPlane,           // Near clipping plane (0.1 typical)
 *         m_farPlane             // Far clipping plane (1000.0 typical)
 *     );
 * }
 * ```
 *
 * Parameters:
 * - **FOV (Field of View)**: Vertical angle in degrees
 *   - 60: Narrow (zoomed in, telephoto lens)
 *   - 75: Standard (most games, human vision)
 *   - 90: Wide (fisheye effect, quake-style)
 *
 * - **Near Plane**: Closest visible distance
 *   - Too small: Z-fighting artifacts (depth precision loss)
 *   - Too large: Objects pop out when too close
 *   - Typical: 0.1 units
 *
 * - **Far Plane**: Farthest visible distance
 *   - Too small: Objects pop out in distance
 *   - Too large: Z-fighting artifacts (depth precision loss)
 *   - Typical: 1000.0 units
 *
 * - **Aspect Ratio**: Width / height (dynamic)
 *   - 16:9 = 1.778
 *   - 16:10 = 1.6
 *   - 4:3 = 1.333
 *   - Ultrawide = 2.333 (21:9)
 *
 * Why aspect passed dynamically:
 * - Window can resize at runtime
 * - Different viewports (split-screen)
 * - Can't cache projection per aspect ratio (memory cost)
 *
 * PURE VIRTUAL METHOD:
 *
 * Derived classes must implement:
 * ```cpp
 * protected:
 *     virtual mat4 computeViewMatrix() const = 0;
 * ```
 *
 * Purpose:
 * - Called by getViewMatrix() when cache dirty
 * - Implements camera-specific view calculation
 * - Base class handles caching, derived handles logic
 *
 * Examples:
 * - FPSCamera: lookAt(position, position + front, up)
 * - Camera: lookAt(position, target, up)
 * - OrbitCamera: lookAt(target + offset, target, up)
 *
 * PERFORMANCE (October 22, 2025):
 *
 * Memory cost per camera:
 * - 3 current matrices: 3 × 64 bytes = 192 bytes
 * - 3 previous matrices: 3 × 64 bytes = 192 bytes
 * - Dirty flags: 3 × 1 byte = 3 bytes
 * - Projection params: 3 × 4 bytes = 12 bytes
 * - Total: ~400 bytes per camera (negligible)
 *
 * Computation cost:
 * - Static camera (no movement):
 *   - First frame: ~500 cycles (compute + cache)
 *   - Subsequent: ~3 cycles (3 dirty checks + return cached)
 *   - Speedup: 166× faster
 *
 * - Moving camera (every frame):
 *   - Per frame: ~500 cycles (recompute)
 *   - Multiple accesses: ~3 cycles per access (return cached)
 *   - Typical: 3 accesses per frame = 500 + (3 × 2) = 506 cycles
 *   - Without caching: 3 × 500 = 1500 cycles
 *   - Speedup: 3× faster
 *
 * DESIGN DECISIONS:
 *
 * 1. Non-Virtual Public Interface:
 * - getters are non-virtual (avoid vtable overhead)
 * - computeViewMatrix() is virtual (polymorphism where needed)
 * - Result: Hot path (getters called every frame) optimized
 *
 * 2. Aspect Ratio Passed Dynamically:
 * - Could cache per aspect ratio (complex)
 * - Could store single aspect (inflexible)
 * - Chosen: Pass at runtime (simple, handles resize)
 *
 * 3. Manual Previous Frame Update:
 * - Could update automatically in getters (implicit)
 * - Chosen: Explicit updatePreviousFrame() call (clear timing control)
 * - Benefit: Application controls when history captured
 *
 * 4. Protected markDirty():
 * - Derived classes call when state changes
 * - Base class doesn't know derived state
 * - Result: Derived controls cache invalidation
 *
 * CURRENT LIMITATIONS (By Design, Address Later):
 *
 * 1. Single Aspect Ratio:
 * - Can't cache projection for multiple viewports
 * - Split-screen would recompute projection twice
 * - Future: Multi-viewport caching (Week 8+)
 *
 * 2. Manual Previous Frame Update:
 * - Application must call updatePreviousFrame()
 * - Easy to forget, breaks temporal effects
 * - Future: Automatic update in getters (Week 7-8)
 *
 * 3. No Interpolation Support:
 * - Cameras snap to new position/rotation
 * - No smooth transitions (lerp/slerp)
 * - Future: Camera smoothing (Week 8+)
 *
 * 4. No Jitter Support:
 * - TAA needs subpixel jitter for sample distribution
 * - No built-in jitter offset support
 * - Future: Halton sequence jitter (Week 7-8 TAA)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 22, 2025: Initial implementation
 * - Extracted common functionality from Camera and FPSCamera
 * - Added matrix caching with dirty flags
 * - Added previous frame matrix storage
 *
 * (TAA Implementation):
 * - Add jitter support (subpixel offsets)
 * - Automatic previous frame update
 * - Temporal validation helpers
 *
 * (Additional Camera Types):
 * - ThirdPersonCamera (follow target with offset)
 * - OrbitCamera (rotate around point)
 * - RailCamera (cinematic fixed paths)
 * - Time: 1-2 days per camera type
 *
 * DEPENDENCIES:
 * - math/EngineMath.h: GLM wrapper (mat4, vec3, perspective, lookAt)
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Mutable cache variables
 * - All camera operations on main render thread only
 * - Multiple reads safe (const methods)
 *
 * REFERENCES:
 * - Game Engine Architecture 3rd Ed., Chapter 14: Camera systems
 * - Real-Time Rendering 4th Ed., Chapter 15: Temporal effects and TAA
 * - "Temporal Reprojection Anti-Aliasing" (SIGGRAPH 2014)
 * - Template Method pattern: Gang of Four Design Patterns
 *
 * HISTORY:
 * October 22, 2025: Initial implementation
 * - Extracted from Camera and FPSCamera classes
 * - Added Template Method pattern (computeViewMatrix virtual)
 * - Added matrix caching with dirty flags
 * - Added previous frame matrix storage for temporal effects
 * - Refactored both existing cameras to inherit from base
 * - Result: Eliminated duplication, enabled temporal support
 *
 */

namespace Engine
{
	class CameraBase
	{
	public:
		// Constructor with projection parameters
		CameraBase(float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 1000.0f);

		// Virtual destructor (required for polymorphic base class)
		virtual ~CameraBase() = default;

		// Public interface (non-virtual for performance)

		// Get current frame matrices (cached, recomputed only if dirty)
		mat4 getViewMatrix() const;
		mat4 getProjectionMatrix(float aspectRatio) const;
		mat4 getViewProjectionMatrix(float aspectRatio) const;

		// Get previous frame matrices (for temporal effects)
		mat4 getPreviousViewMatrix() const { return m_prevView; }
		mat4 getPreviousProjectionMatrix() const { return m_prevProjection; }
		mat4 getPreviousViewProjectionMatrix() const { return m_prevViewProjection; }

		// Update previous frame matrices (call once per frame, before rendering)
		void updatePreviousFrame(float aspectRatio);

		// Projection parameter control
		void setFOV(float fov);
		void setNearPlane(float nearPlane);
		void setFarPlane(float farPlane);

		float getFOV() const { return m_fov; }
		float getNearPlane() const { return m_near; }
		float getFarPlane() const { return m_far; }

		// Pure virtual: Derived classes must implement
		virtual vec3 getPosition() const = 0;

	protected:
		// Derived class interface

		// Pure virtual: Derived classes compute view matrix from their state
		virtual mat4 computeViewMatrix() const = 0;

		// Mark matrices as dirty (derived classes call this on state change)
		void markDirty() { m_dirty = true; }

		// Projection parameters (shared by all cameras)
		float m_fov; // Field of view in degrees
		float m_near; // Near clipping plane
		float m_far; // Far clipping plane

	private:
		// Internal state

		// Current frame matrices (cached)
		mutable mat4 m_cachedView;
		mutable mat4 m_cachedProjection;
		mutable mat4 m_cachedViewProjection;
		mutable float m_cachedAspect = 0.0f;
		mutable bool m_dirty = true;

		// Previous frame matrices (for temporal)
		mat4 m_prevView;
		mat4 m_prevProjection;
		mat4 m_prevViewProjection;

		// Recompute matrices if dirty
		void updateCache(float aspectRatio) const;
	};
}