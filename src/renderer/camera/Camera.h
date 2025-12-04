#pragma once
#include "math/EngineMath.h"
#include "renderer/camera/CameraBase.h"

/*
 * Camera.h
 *
 * PURPOSE:
 * Basic position-target camera with optional orbit mode. Simplest camera type - specify
 * position and look-at target, camera points there. Derived from LearnOpenGL tutorial,
 * adapted to CameraBase inheritance (November 15, 2025). Primarily used for static scenes,
 * cutscenes, or simple prototyping where manual camera control unnecessary.
 *
 * DESIGN RATIONALE (October 6, 2025 - November 15, 2025):
 * Problem: Need basic camera for initial rendering tests. FPSCamera too complex for static
 * scenes. Need something simple - just position and target.
 *
 * Solution: Position-target camera from LearnOpenGL tutorial.
 * - October 6: Initial implementation (standalone, LearnOpenGL-based)
 * - November 15: Refactored to inherit from CameraBase (template method pattern)
 *
 * Key Insight: Most basic camera possible - two vectors (position, target) compute view
 * matrix via lookAt. No movement logic, no input handling. Perfect for static scenes.
 *
 * DESIGN PHILOSOPHY:
 * - Simple API: Set position and target, that's it
 * - Static by default: No update() method, no input handling
 * - LearnOpenGL-based: Standard educational camera implementation
 * - CameraBase integration: Inherits caching, temporal support
 *
 * KEY CONCEPTS:
 * 1. Position-Target Model:
 *    - Position: Where camera is in world space
 *    - Target: What camera looks at in world space
 *    - View matrix: glm::lookAt(position, target, up)
 *
 * 2. Up Vector:
 *    - Defines camera roll (tilt)
 *    - Typically vec3(0, 1, 0) for world up
 *    - Can modify for dutch angle effects
 *
 * 3. Orbit Mode (optional):
 *    - Position = target + offset
 *    - Offset calculated from yaw/pitch angles + radius
 *    - Enables rotation around target point
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === BASIC STATIC CAMERA ===
 * Camera camera;
 * camera.setPosition(vec3(0, 5, 10));  // Behind and above origin
 * camera.setTarget(vec3(0, 0, 0));     // Look at origin
 *
 * // Render (camera doesn't move)
 * scene.render(camera, shader, window, renderer);
 *
 * // === ORBIT CAMERA (Manual Rotation) ===
 * Camera camera;
 * camera.setTarget(vec3(0, 1, 0));     // Look at object at height 1
 * camera.setPosition(vec3(5, 2, 5));   // Initial position
 *
 * // Rotate around target (manual, in application code)
 * float angle = glfwGetTime();
 * float radius = 10.0f;
 * camera.setPosition(vec3(
 *     sin(angle) * radius,
 *     2.0f,
 *     cos(angle) * radius
 * ));
 *
 * // === CUTSCENE CAMERA (Interpolated) ===
 * Camera camera;
 * vec3 startPos = vec3(0, 5, 10);
 * vec3 endPos = vec3(10, 5, 0);
 * vec3 target = vec3(0, 1, 0);
 *
 * float t = cutsceneTime / cutsceneDuration;
 * camera.setPosition(glm::mix(startPos, endPos, t));
 * camera.setTarget(target);
 * ```
 *
 * VIEW MATRIX CALCULATION:
 *
 * ```cpp
 * mat4 Camera::computeViewMatrix() const {
 *     return glm::lookAt(m_position, m_target, m_up);
 * }
 * ```
 *
 * GLM lookAt breakdown:
 * 1. Forward = normalize(target - position)
 * 2. Right = normalize(cross(forward, worldUp))
 * 3. Up = cross(right, forward)
 * 4. Build view matrix from right, up, forward vectors
 *
 * Result: Transforms world space -> camera space
 *
 * COMPARISON WITH FPSCAMERA:
 *
 * Camera (position-target):
 * - Simple: Two vectors (position, target)
 * - Static: No input handling, no update loop
 * - Use cases: Static scenes, cutscenes, prototyping
 * - Control: Application sets position/target directly
 *
 * FPSCamera (first-person):
 * - Complex: Position + yaw/pitch angles + front/right/up vectors
 * - Dynamic: WASD movement, mouse look, update loop
 * - Use cases: Player camera, free fly, interactive exploration
 * - Control: Internal input handling, smooth movement
 *
 * WHEN TO USE EACH:
 * - **Camera**: Static shots, cutscenes, orbital views, menu cameras
 * - **FPSCamera**: Player control, editor mode, gameplay cameras
 *
 * CURRENT STATE (November 15, 2025):
 * - Inherits from CameraBase (matrix caching, temporal support)
 * - Position-target model (LearnOpenGL-based)
 * - World up vector (vec3(0, 1, 0) default)
 * - No input handling (application controls position/target)
 *
 * CURRENT LIMITATIONS (By Design):
 * 1. No Input Handling:
 * - Application must set position/target manually
 * - No built-in orbit controls
 * - Future: Add orbit mode helpers (Week 5+)
 *
 * 2. No Smooth Movement:
 * - Position/target snap immediately
 * - No interpolation or easing
 * - Future: Add lerp helpers (Week 8+)
 *
 * 3. No Constraints:
 * - Can position anywhere (even inside geometry)
 * - No collision detection
 * - Acceptable for static cameras
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 6, 2025: Initial implementation
 * - Basic position-target camera
 * - LearnOpenGL tutorial-based
 * - Standalone class (no inheritance)
 *
 * November 15, 2025: CameraBase refactor
 * - Refactored to inherit from CameraBase
 * - Gained matrix caching (3-166× speedup)
 * - Gained temporal support (TAA-ready)
 *
 * (Future Enhancements):
 * - Orbit mode helpers (setOrbitRadius, rotateAround)
 * - Smooth transitions (lerp, ease functions)
 * - Constraint helpers (keepAboveGround, avoidGeometry)
 *
 * DEPENDENCIES:
 * - camera/CameraBase.h: Base class (caching, temporal, projection)
 * - math/EngineMath.h: GLM wrapper (vec3, mat4, lookAt)
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Inherits mutable cache from CameraBase
 * - All operations on main render thread only
 *
 * REFERENCES:
 * - LearnOpenGL.com: Camera tutorial (position-target model)
 * - CameraBase.h: Inheritance documentation
 *
 * HISTORY:
 * October 6, 2025: Initial implementation
 * - Basic position-target camera
 * - LearnOpenGL tutorial code
 * - Standalone class
 *
 * November 15, 2025: CameraBase integration
 * - Refactored to inherit from CameraBase
 * - Implements computeViewMatrix() virtual method
 * - Gained caching and temporal support
 *
 */

namespace Engine
{
	class Camera : public CameraBase
	{
	public:
		// Constructor with all parameters
		Camera(const vec3& position,
			const vec3& target,
			float fov = 45.0f,
			float nearPlane = 0.1f,
			float farPlane = 1000.0f);

		// Position control
		void setPosition(const vec3& position);
		void setTarget(const vec3& target);
		void setUpVector(const vec3& up);

		// Getters
		vec3 getPosition() const override { return m_position; }
		vec3 getTarget() const { return m_target; }
		vec3 getUpVector() const { return m_up; }

		// Utility vectors
		vec3 getForward() const;
		vec3 getRight() const;
		vec3 getUp() const;

	protected:
		// Implement pure virtual from CameraBase
		mat4 computeViewMatrix() const override;

	private:
		// Camera transform
		vec3 m_position;
		vec3 m_target;
		vec3 m_up;
	};
}