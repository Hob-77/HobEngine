#pragma once
#include "math/EngineMath.h"
#include "renderer/camera/CameraBase.h"

/*
 * FPSCamera.h
 *
 * PURPOSE:
 * First-person shooter style camera with WASD movement and mouse look. Provides intuitive
 * controls for free exploration of 3D environments. Integrates with Input system for smooth,
 * frame-rate independent movement. Primary camera for gameplay, editor mode, and interactive
 * exploration.
 *
 * DESIGN RATIONALE (October 8, 2025):
 * Problem: Basic Camera class is static (position-target). Need interactive camera for testing
 * rendering features, exploring scenes, gameplay prototyping. Need FPS-style controls - WASD
 * movement, mouse look, intuitive and familiar.
 *
 * Solution: FPS camera based on OGLDEV tutorial series.
 * - Euler angles (yaw/pitch) for intuitive mouse control
 * - Forward/right/up vectors derived from angles
 * - WASD movement along camera axes
 * - Pitch clamping prevents gimbal lock
 * - Inherits caching/temporal from CameraBase (November 15 refactor)
 *
 * Key Insight: OGLDEV's approach - Euler angles map naturally to mouse movement. Pitch clamping
 * at ±89 avoids gimbal lock while keeping intuitive controls. Forward vector calculated from
 * angles eliminates need for quaternion complexity in typical FPS scenarios.
 *
 * DESIGN PHILOSOPHY:
 * - Intuitive controls: WASD + mouse (familiar to all gamers)
 * - Euler angles: Simple, debuggable, maps directly to mouse movement
 * - Frame-rate independent: Movement scaled by deltaTime
 * - Configurable: MovementSettings for speed, sensitivity, fly mode
 * - Input integration: Works with centralized Input system
 *
 * KEY CONCEPTS:
 * 1. Euler Angle System:
 *    - Yaw (horizontal): Look left/right, unrestricted rotation
 *    - Pitch (vertical): Look up/down, clamped ±89 (no gimbal lock)
 *    - Roll: Not used (would cause disorienting screen tilt)
 *
 * 2. Camera Vectors (Derived from Angles):
 *    - Front: Direction camera faces (calculated from yaw/pitch)
 *    - Right: Perpendicular to front (for strafing)
 *    - Up: Perpendicular to front/right (for vertical movement)
 *
 * 3. Coordinate System (OpenGL Convention):
 *    - Forward: -Z direction in camera space
 *    - Right: +X direction in camera space
 *    - Up: +Y direction (world up)
 *    - Default yaw: -90 (look down -Z axis)
 *
 * 4. Movement Model:
 *    - Forward/Back: Move along front vector
 *    - Strafe: Move along right vector
 *    - Vertical: Move along world up (fly mode)
 *    - Sprint: Multiply speed by sprintMultiplier
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === CAMERA SETUP ===
 * FPSCamera camera(
 *     vec3(0, 2, 5),  // Starting position (slightly elevated, behind origin)
 *     -90.0f,         // Yaw (look forward down -Z)
 *     0.0f            // Pitch (level, horizontal)
 * );
 *
 * // Configure movement
 * FPSCamera::MovementSettings settings;
 * settings.walkSpeed = 5.0f;           // 5 units/second
 * settings.sprintMultiplier = 2.0f;    // Sprint = 10 units/second
 * settings.mouseSensitivity = 0.1f;    // 0.1 degrees per pixel
 * settings.flyMode = true;             // Enable vertical movement
 * settings.invertY = false;            // Normal Y-axis
 * camera.setMovementSettings(settings);
 *
 * // === UPDATE LOOP (Process Input) ===
 * void onUpdate(float deltaTime) {
 *     // Process keyboard movement (WASD, Space, Ctrl, Shift)
 *     camera.processKeyboard(deltaTime);
 *
 *     // Process mouse look
 *     vec2 mouseDelta = Input::getMouseDelta();
 *     camera.processMouseMovement(mouseDelta.x, mouseDelta.y);
 * }
 *
 * // === RENDER LOOP ===
 * void onRender() {
 *     float aspect = window.getAspectRatio();
 *
 *     // Update temporal history (BEFORE rendering, for TAA/motion blur)
 *     camera.updatePreviousFrame(aspect);
 *
 *     // Set camera matrices
 *     shader.setUniform("u_View", camera.getViewMatrix());
 *     shader.setUniform("u_Projection", camera.getProjectionMatrix(aspect));
 *     shader.setUniform("u_CameraPos", camera.getPosition());
 *
 *     // Render scene
 *     scene.render(camera, shader, window, renderer);
 * }
 *
 * // === ADVANCED: MOTION BLUR ===
 * void applyMotionBlur() {
 *     motionBlurShader.setUniform("u_CurrentViewProj",
 *                                  camera.getViewProjectionMatrix(aspect));
 *     motionBlurShader.setUniform("u_PreviousViewProj",
 *                                  camera.getPreviousViewProjectionMatrix());
 * }
 * ```
 *
 * EULER ANGLES - Detailed Breakdown:
 *
 * Yaw (horizontal rotation, around Y axis):
 * - Range: Unrestricted (-infinity to +infinity, wraps at 360)
 * - Controlled by: Mouse X movement
 * - Effect: Look left (negative) or right (positive)
 * - Default: -90 (camera faces down -Z axis, OpenGL convention)
 * - Example: yaw = 0 -> look down +X, yaw = -90 -> look down -Z
 *
 * Pitch (vertical rotation, around X axis):
 * - Range: Clamped -89 to +89 (prevents gimbal lock)
 * - Controlled by: Mouse Y movement
 * - Effect: Look up (positive) or down (negative)
 * - Default: 0 (level, horizontal)
 * - Clamping: At ±90, camera would flip upside-down (gimbal lock)
 * - Example: pitch = +45 -> look 45 upward, pitch = -45 -> look 45 downward
 *
 * Why clamp pitch at ±89?
 * - Gimbal lock: At ±90, yaw and roll become same axis (lose degree of freedom)
 * - Disorientation: Looking straight up/down then rotating yaw causes screen flip
 * - Solution: Clamp at ±89 keeps intuitive controls without mathematical singularity
 *
 * Camera vector calculation (from yaw/pitch):
 * ```cpp
 * void updateCameraVectors() {
 *     // Convert degrees to radians
 *     float yawRad = glm::radians(m_yaw);
 *     float pitchRad = glm::radians(m_pitch);
 *
 *     // Calculate front vector from spherical coordinates
 *     m_front.x = cos(pitchRad) * cos(yawRad);
 *     m_front.y = sin(pitchRad);
 *     m_front.z = cos(pitchRad) * sin(yawRad);
 *     m_front = glm::normalize(m_front);
 *
 *     // Calculate right vector (perpendicular to front and world up)
 *     m_right = glm::normalize(glm::cross(m_front, m_worldUp));
 *
 *     // Calculate up vector (perpendicular to right and front)
 *     m_up = glm::normalize(glm::cross(m_right, m_front));
 * }
 * ```
 *
 * COORDINATE SYSTEM (OpenGL Convention):
 *
 * Default orientation (yaw=-90, pitch=0):
 * - Front: (0, 0, -1) -> Looking down -Z axis
 * - Right: (1, 0, 0) -> +X axis
 * - Up: (0, 1, 0) -> +Y axis
 *
 * Why yaw=-90 default?
 * - OpenGL convention: Camera looks down -Z by default
 * - yaw=0 would look down +X axis (unconventional)
 * - yaw=-90 looks down -Z axis (standard forward direction)
 *
 * Rotation examples:
 * - yaw=0, pitch=0: Look down +X axis (east)
 * - yaw=-90, pitch=0: Look down -Z axis (forward, default)
 * - yaw=-180, pitch=0: Look down -X axis (west)
 * - yaw=-90, pitch=+45: Look forward and up
 *
 * MOVEMENT SYSTEM:
 *
 * Movement controls (keyboard):
 * - **W**: Forward (move along front vector)
 * - **S**: Backward (move opposite front vector)
 * - **A**: Strafe left (move opposite right vector)
 * - **D**: Strafe right (move along right vector)
 * - **Space**: Move up (world +Y, fly mode only)
 * - **Left Ctrl**: Move down (world -Y, fly mode only)
 * - **Left Shift**: Sprint (multiply speed)
 *
 * Movement calculation:
 * ```cpp
 * void processKeyboard(float deltaTime) {
 *     float velocity = m_settings.walkSpeed * deltaTime;
 *
 *     // Sprint modifier
 *     if (Input::isKeyPressed(Key::LeftShift)) {
 *         velocity *= m_settings.sprintMultiplier;
 *     }
 *
 *     // Forward/backward
 *     if (Input::isKeyPressed(Key::W)) {
 *         m_position += m_front * velocity;
 *     }
 *     if (Input::isKeyPressed(Key::S)) {
 *         m_position -= m_front * velocity;
 *     }
 *
 *     // Strafe left/right
 *     if (Input::isKeyPressed(Key::A)) {
 *         m_position -= m_right * velocity;
 *     }
 *     if (Input::isKeyPressed(Key::D)) {
 *         m_position += m_right * velocity;
 *     }
 *
 *     // Vertical movement (fly mode)
 *     if (m_settings.flyMode) {
 *         if (Input::isKeyPressed(Key::Space)) {
 *             m_position += m_worldUp * velocity;
 *         }
 *         if (Input::isKeyPressed(Key::LeftControl)) {
 *             m_position -= m_worldUp * velocity;
 *         }
 *     }
 *
 *     markDirty();  // Invalidate cached matrices
 * }
 * ```
 *
 * Mouse look calculation:
 * ```cpp
 * void processMouseMovement(float xoffset, float yoffset) {
 *     // Apply sensitivity
 *     xoffset *= m_settings.mouseSensitivity;
 *     yoffset *= m_settings.mouseSensitivity;
 *
 *     // Invert Y if enabled
 *     if (m_settings.invertY) {
 *         yoffset = -yoffset;
 *     }
 *
 *     // Update angles
 *     m_yaw += xoffset;
 *     m_pitch += yoffset;
 *
 *     // Clamp pitch (prevent gimbal lock)
 *     if (m_pitch > 89.0f) m_pitch = 89.0f;
 *     if (m_pitch < -89.0f) m_pitch = -89.0f;
 *
 *     // Recalculate camera vectors
 *     updateCameraVectors();
 *
 *     markDirty();  // Invalidate cached matrices
 * }
 * ```
 *
 * MOVEMENT SETTINGS:
 *
 * ```cpp
 * struct MovementSettings {
 *     float walkSpeed = 5.0f;           // Base movement speed (units/second)
 *     float sprintMultiplier = 2.0f;    // Sprint speed multiplier
 *     float mouseSensitivity = 0.1f;    // Mouse look sensitivity (degrees/pixel)
 *     bool flyMode = true;              // Enable vertical movement (Space/Ctrl)
 *     bool invertY = false;             // Invert Y-axis (airplane controls)
 * };
 * ```
 *
 * Typical configurations:
 * - **FPS gameplay**: walkSpeed=5, sprint=2.0x, sensitivity=0.1, fly=false
 * - **Editor mode**: walkSpeed=10, sprint=3.0x, sensitivity=0.15, fly=true
 * - **Cinematic**: walkSpeed=2, sprint=1.5x, sensitivity=0.05, fly=true
 *
 * VIEW MATRIX CALCULATION:
 *
 * ```cpp
 * mat4 FPSCamera::computeViewMatrix() const {
 *     return glm::lookAt(m_position, m_position + m_front, m_up);
 * }
 * ```
 *
 * Breakdown:
 * - Eye: m_position (where camera is)
 * - Center: m_position + m_front (where camera looks)
 * - Up: m_up (camera's up vector, perpendicular to front/right)
 *
 * Result: Transforms world space -> camera space
 *
 * COMPARISON WITH CAMERA:
 *
 * FPSCamera (interactive):
 * - Input handling: Built-in WASD + mouse
 * - Control: User-controlled via keyboard/mouse
 * - Update loop: Continuous processKeyboard/processMouseMovement
 * - Use cases: Gameplay, editor, exploration
 *
 * Camera (static):
 * - Input handling: None (application sets position/target)
 * - Control: Programmatic (application code)
 * - Update loop: None (set and forget)
 * - Use cases: Cutscenes, menus, static shots
 *
 * CURRENT STATE (November 15, 2025):
 * - Inherits from CameraBase (matrix caching, temporal support)
 * - Euler angle system (yaw/pitch, no roll)
 * - WASD movement + mouse look
 * - Frame-rate independent (deltaTime scaling)
 * - Configurable movement settings
 * - Input system integration
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Euler Angles Only:
 * - Gimbal lock possible if pitch clamping removed
 * - No smooth interpolation (quaternions better for this)
 * - Acceptable: Clamping prevents gimbal lock in practice
 *
 * 2. No Collision Detection:
 * - Camera can move through walls/geometry
 * - No ground detection (fly mode always)
 * - Future: Add collision system integration 
 *
 * 3. No Smoothing/Acceleration:
 * - Movement is instant (no acceleration/deceleration)
 * - Mouse look is instant (no smoothing)
 * - Future: Add smoothing for cinematic feel 
 *
 * 4. No Head Bob:
 * - No walking animation (static height)
 * - Future: Add head bob option 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 8, 2025: Initial implementation
 * - FPS camera with WASD + mouse (OGLDEV-based)
 * - Euler angle system (yaw/pitch)
 * - Frame-rate independent movement
 * - Standalone class
 *
 * November 15, 2025: CameraBase refactor
 * - Refactored to inherit from CameraBase
 * - Gained matrix caching (3-166× speedup)
 * - Gained temporal support (TAA-ready)
 *
 * (Future Enhancements):
 * - Collision detection (raycast or capsule)
 * - Movement smoothing (acceleration/deceleration)
 * - Head bob animation
 * - Footstep sounds integration
 *
 * (Cinematic Features):
 * - Camera shake effects
 * - Smooth transitions (lerp/slerp)
 * - Look-at constraints
 *
 * DEPENDENCIES:
 * - camera/CameraBase.h: Base class (caching, temporal, projection)
 * - core/Input.h: Keyboard/mouse input queries
 * - math/EngineMath.h: GLM wrapper (vec3, mat4, lookAt, cross, normalize)
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Mutable state, inherits cache from CameraBase
 * - All operations on main thread only
 * - Input system assumes main thread
 *
 * REFERENCES:
 * - OGLDEV OpenGL Tutorial Series: Camera tutorial (primary inspiration)
 * - LearnOpenGL.com: Camera tutorial (supplementary)
 * - CameraBase.h: Inheritance documentation
 *
 * HISTORY:
 * October 8, 2025: Initial implementation
 * - FPS camera based on OGLDEV tutorial
 * - Euler angles (yaw/pitch, clamped)
 * - WASD movement, mouse look
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
    class FPSCamera : public CameraBase
    {
    public:
        // Movement settings
        struct MovementSettings
        {
            float walkSpeed = 5.0f;        // Units per second
            float sprintMultiplier = 2.0f; // Sprint speed multiplier
            float mouseSensitivity = 0.25f; // Mouse look sensitivity
            bool flyMode = true;           // Allow vertical movement
            bool invertY = false;          // Invert Y axis (airplane controls)
        };

        // Constructor
        FPSCamera(const vec3& position = vec3(0.0f, 2.0f, 5.0f),
            float yaw = -90.0f,    // Look down -Z by default
            float pitch = 0.0f,
            float fov = 45.0f,
            float nearPlane = 0.1f,
            float farPlane = 1000.0f);

        // Input processing (call these in onUpdate)
        void processKeyboard(float deltaTime);
        void processMouseMovement(float deltaX, float deltaY, bool constrainPitch = true);
        void processMouseScroll(float yOffset); // Zoom via FOV adjustment

        // Position and orientation
        vec3 getPosition() const override { return m_position; }
        vec3 getFront() const { return m_front; }
        vec3 getRight() const { return m_right; }
        vec3 getUp() const { return m_up; }

        // Euler angles
        float getYaw() const { return m_yaw; }
        float getPitch() const { return m_pitch; }

        // Settings
        void setPosition(const vec3& position);
        void setMovementSettings(const MovementSettings& settings) { m_settings = settings; }
        MovementSettings& getMovementSettings() { return m_settings; }

        // Reset camera to default position/orientation
        void reset(const vec3& position = vec3(0.0f, 2.0f, 5.0f),
            float yaw = -90.0f,
            float pitch = 0.0f);

    protected:
        // Pure virtual from CameraBase
        mat4 computeViewMatrix() const override;

    private:
        // Update camera vectors based on Euler angles
        void updateCameraVectors();

    private:
        // Camera position and orientation
        vec3 m_position;
        vec3 m_front;     // Direction camera is facing (normalized)
        vec3 m_up;        // Up vector relative to camera
        vec3 m_right;     // Right vector relative to camera
        vec3 m_worldUp;   // World up (usually Y-axis)

        // Euler angles (in degrees)
        float m_yaw;      // Rotation around Y axis
        float m_pitch;    // Rotation around X axis

        // Movement settings
        MovementSettings m_settings;
    };
}