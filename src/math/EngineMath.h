#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <format>

/*
 * EngineMath.h
 *
 * PURPOSE:
 * Centralized math library wrapping GLM (OpenGL Mathematics). Provides vector, matrix,
 * and quaternion types with convenient aliases. Enables consistent math operations across
 * the entire engine. Single include point for all math functionality. Custom formatters
 * for logging integration.
 *
 * DESIGN RATIONALE (September 27, 2025):
 * Problem: GLM types verbose (glm::vec3, glm::mat4). GLM headers scattered throughout
 * codebase. No consistent logging format for vectors/matrices. Difficult to migrate math
 * library if needed.
 *
 * Solution: Thin wrapper over GLM with convenient aliases and utilities.
 * - Type aliases: vec3 instead of glm::vec3 (shorter, cleaner)
 * - Single include: #include "EngineMath.h" (not scattered GLM headers)
 * - Custom formatters: LOG_INFO("pos: {}", pos) works (std::format integration)
 * - Easy migration: Change one file if switching math libraries
 * - Add functions as needed: Minimal overhead, grow organically
 *
 * Key Insight: Math library fundamental to engine (used everywhere). Thin wrapper provides
 * convenience without abstraction overhead. GLM industry-standard (battle-tested, SIMD-
 * optimized, GLSL-compatible). Type aliases reduce verbosity. Custom formatters essential
 * for debugging (vectors/matrices in logs).
 *
 * DESIGN PHILOSOPHY:
 * - Thin wrapper: Minimal abstraction, direct GLM access
 * - Convenient aliases: Shorter type names (vec3, mat4)
 * - Single include: Centralized math imports
 * - Custom formatters: Better logging experience
 * - Grow organically: Add functions as needed (not upfront)
 *
 * KEY CONCEPTS:
 * 1. Type Aliases:
 *    - vec2, vec3, vec4: Floating-point vectors
 *    - ivec2, ivec3: Integer vectors
 *    - mat4: 4x4 transformation matrix
 *    - quat: Quaternion (smooth rotations)
 *
 * 2. GLM Functions:
 *    - Math: radians, degrees, length, normalize, dot, cross
 *    - Transform: translate, rotate, scale, lookAt, perspective
 *    - Utility: value_ptr, min, max, cos, sin
 *
 * 3. Coordinate System (Right-Handed):
 *    - +X: Right
 *    - +Y: Up
 *    - +Z: Toward camera (out of screen)
 *    - OpenGL/GLSL convention
 *
 * 4. Matrix Layout:
 *    - Column-major (OpenGL/GLSL standard)
 *    - Matches shader expectations
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === VECTOR OPERATIONS ===
 * vec3 position(5.0f, 2.0f, 3.0f);
 * vec3 direction(-1.0f, 0.0f, 0.0f);
 *
 * // Length (magnitude)
 * float dist = length(position);  // Distance from origin
 *
 * // Normalize (unit vector)
 * vec3 normalized = normalize(direction);  // Length = 1
 *
 * // Dot product (projection)
 * float alignment = dot(position, direction);  // How aligned?
 *
 * // Cross product (perpendicular vector)
 * vec3 perpendicular = cross(position, direction);
 *
 * // === MATRIX TRANSFORMATIONS ===
 * mat4 model = mat4(1.0f);  // Identity matrix
 *
 * // Translate (move)
 * model = translate(model, vec3(0, 1, 0));  // Move up 1 unit
 *
 * // Rotate (angle + axis)
 * model = rotate(model, radians(45.0f), vec3(0, 1, 0));  // Rotate 45 degrees around Y
 *
 * // Scale (resize)
 * model = scale(model, vec3(2, 1, 1));  // Scale X by 2
 *
 * // === CAMERA MATRICES ===
 * // View matrix (camera transform)
 * mat4 view = lookAt(
 *     vec3(0, 5, 10),  // Eye position (camera location)
 *     vec3(0, 0, 0),   // Center (look at point)
 *     vec3(0, 1, 0)    // Up direction
 * );
 *
 * // Projection matrix (perspective)
 * mat4 proj = perspective(
 *     radians(45.0f),  // FOV (field of view)
 *     16.0f / 9.0f,    // Aspect ratio
 *     0.1f,            // Near plane
 *     100.0f           // Far plane
 * );
 *
 * // === LOGGING WITH FORMATTERS ===
 * vec3 pos(1.5f, 2.3f, 4.7f);
 * LOG_INFO("Position: {}", pos);
 * // Output: "Position: (1.50, 2.30, 4.70)"
 *
 * vec2 uv(0.25f, 0.75f);
 * LOG_INFO("UV: {}", uv);
 * // Output: "UV: (0.25, 0.75)"
 * ```
 *
 * TYPE ALIASES - What They Mean:
 *
 * Floating-point vectors:
 * - vec2: 2D (x, y) - UV coordinates, screen positions, 2D directions
 * - vec3: 3D (x, y, z) - World positions, directions, RGB colors, normals
 * - vec4: 4D (x, y, z, w) - RGBA colors, homogeneous coordinates, planes
 *
 * Integer vectors:
 * - ivec2: 2D integer - Window dimensions (1920x1080), grid positions
 * - ivec3: 3D integer - Voxel coordinates, grid indices, array indices
 *
 * Matrices:
 * - mat4: 4x4 matrix - Model/View/Projection transforms, any affine transform
 *
 * Rotations:
 * - quat: Quaternion - Smooth rotations (avoids gimbal lock, future use)
 *
 * VECTOR COMPONENT ACCESS:
 *
 * Multiple access methods (same memory, different names):
 * ```cpp
 * vec3 v(1.0f, 2.0f, 3.0f);
 *
 * // Cartesian (3D space)
 * float x = v.x;  // 1.0
 * float y = v.y;  // 2.0
 * float z = v.z;  // 3.0
 *
 * // Color (RGB/RGBA)
 * float r = v.r;  // 1.0 (same as v.x)
 * float g = v.g;  // 2.0 (same as v.y)
 * float b = v.b;  // 3.0 (same as v.z)
 *
 * // Texture (UV coordinates)
 * float s = v.s;  // 1.0 (same as v.x)
 * float t = v.t;  // 2.0 (same as v.y)
 *
 * // Array (index-based)
 * float x = v[0];  // 1.0
 * float y = v[1];  // 2.0
 * float z = v[2];  // 3.0
 * ```
 *
 * COMMON FUNCTIONS:
 *
 * Angle conversion:
 * - radians(degrees): Convert degrees to radians (for trig functions)
 * - degrees(radians): Convert radians to degrees (for display)
 *
 * Vector operations:
 * - length(v): Magnitude (distance from origin)
 * - normalize(v): Unit vector (length = 1, preserves direction)
 * - dot(a, b): Dot product (projection, angle between vectors)
 * - cross(a, b): Cross product (perpendicular vector, right-hand rule)
 *
 * Matrix operations:
 * - translate(m, v): Apply translation (move)
 * - rotate(m, angle, axis): Apply rotation (spin around axis)
 * - scale(m, v): Apply scale (resize)
 * - lookAt(eye, center, up): View matrix for camera (where camera looks)
 * - perspective(fov, aspect, near, far): Projection matrix (perspective view)
 *
 * Utility:
 * - value_ptr(v): Get pointer to data (for OpenGL uniform uploads)
 * - min(a, b): Component-wise minimum
 * - max(a, b): Component-wise maximum
 * - cos(x), sin(x): Trigonometry (radians)
 *
 * CUSTOM FORMATTERS - Logging Integration:
 *
 * ```cpp
 * // vec3 formatter (2 decimal places)
 * vec3 pos(1.567f, 2.345f, 4.789f);
 * LOG_INFO("Position: {}", pos);
 * // Output: "Position: (1.57, 2.35, 4.79)"
 *
 * // vec2 formatter (2 decimal places)
 * vec2 uv(0.12345f, 0.67890f);
 * LOG_INFO("UV: {}", uv);
 * // Output: "UV: (0.12, 0.68)"
 *
 * // Multiple vectors
 * LOG_INFO("Camera: eye={}, target={}", eye, target);
 * // Output: "Camera: eye=(0.00, 5.00, 10.00), target=(0.00, 0.00, 0.00)"
 * ```
 *
 * Benefits:
 * - Readable logs (no manual formatting)
 * - Consistent precision (2 decimal places)
 * - Integrates with std::format (C++20)
 *
 * WHY GLM?
 *
 * Industry standard:
 * - Used by millions of projects (games, engines, tools)
 * - Battle-tested (stable, reliable)
 * - Well-documented (comprehensive resources)
 *
 * Performance:
 * - SIMD optimized (SSE, AVX when available)
 * - Header-only (compiler can inline aggressively)
 * - Minimal overhead (thin wrapper over CPU instructions)
 *
 * Compatibility:
 * - GLSL syntax (matches shader code exactly)
 * - OpenGL conventions (column-major matrices)
 * - Cross-platform (Windows, Linux, macOS)
 *
 * Functionality:
 * - Extensive (vectors, matrices, quaternions, geometry)
 * - Extendable (easy to add custom utilities)
 * - Future-proof (active development, C++20 support)
 *
 * COORDINATE SYSTEM - Right-Handed:
 *
 * ```
 *       +Y (Up)
 *        |
 *        |
 *        |
 *        +-------> +X (Right)
 *       /
 *      /
 *   +Z (Toward camera)
 * ```
 *
 * Convention: OpenGL/GLSL standard (right-handed)
 * - +X: Right
 * - +Y: Up
 * - +Z: Toward camera (out of screen)
 * - Rotation: Counter-clockwise positive (right-hand rule)
 *
 * CURRENT STATE (September 27, 2025 - Ongoing):
 * - Type aliases (vec2/3/4, mat4, quat, ivec2/3)
 * - Common functions (math, transform, utility)
 * - Custom formatters (vec2, vec3 for logging)
 * - Single include point (EngineMath.h)
 * - Grow organically (add functions as needed)
 * - Status: Production-ready, minimal but sufficient
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. No vec4/mat4 Formatters:
 * - Would be verbose (16 floats for mat4)
 * - Add if needed (not common in logs)
 *
 * 2. No Custom Geometry Types:
 * - No Ray, Plane, Frustum, Sphere classes
 * - Use raw GLM types or add when needed
 *
 * 3. No Interpolation Utilities:
 * - No lerp, slerp, smoothstep wrappers
 * - GLM has these (use directly if needed)
 *
 * 4. No Random Generators:
 * - No randomVec3, randomInSphere helpers
 * - Add when needed (not common enough yet)
 *
 * 5. No Explicit SIMD:
 * - Relies on compiler optimizations
 * - GLM uses SIMD when available (SSE, AVX)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 27, 2025: Initial implementation
 * - Type aliases (vec2/3/4, mat4, quat)
 * - Common functions (math, transform)
 * - Custom formatters (vec2, vec3)
 * - Status: Production-ready foundation
 *
 * Ongoing (Add as Needed):
 * - Additional formatters (vec4, mat4 if needed)
 * - Utility functions (lerp, smoothstep, etc.)
 * - Custom geometry types (Ray, Plane, Sphere)
 * - Random generators (randomVec3, etc.)
 * - Time: 1-2 hours per addition (as needed)
 *
 * DEPENDENCIES:
 * - <glm/glm.hpp>: Core GLM (vectors, matrices)
 * - <glm/gtc/matrix_transform.hpp>: Transform functions
 * - <glm/gtc/type_ptr.hpp>: value_ptr (OpenGL integration)
 * - <format>: C++20 formatting (custom formatters)
 *
 * THREAD SAFETY:
 * - Thread-safe: Pure math operations, no shared state
 * - GLM functions: Stateless, thread-safe
 * - Can use on any thread (rendering, physics, AI)
 *
 * REFERENCES:
 * - GLM documentation: https://github.com/g-truc/glm
 * - OpenGL Mathematics: https://glm.g-truc.net/
 * - GLSL specification: Syntax compatibility reference
 *
 * HISTORY:
 * September 27, 2025: Initial implementation
 * - Type aliases for GLM types (vec2/3/4, mat4, quat, ivec2/3)
 * - Common function imports (math, transform, utility)
 * - Custom formatters (vec2, vec3 for std::format)
 * - Single include point (EngineMath.h)
 * - Grow organically philosophy (add functions as needed)
 *
 * Ongoing:
 * - Add functions as needed (minimal overhead, practical approach)
 * - No preemptive features (YAGNI - You Ain't Gonna Need It)
 * - Formatters added for debugging convenience
 *
 */

namespace Engine 
{
    // Core types
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat4 = glm::mat4;
    using quat = glm::quat;

    // Integer variants
    using ivec2 = glm::ivec2;
    using ivec3 = glm::ivec3;

    // Bring in common functions
    using glm::radians;
    using glm::degrees;
    using glm::length;
    using glm::normalize;
    using glm::dot;
    using glm::cross;

    // For matrices
    using glm::translate;
    using glm::rotate;
    using glm::scale;
    using glm::lookAt;
    using glm::perspective;

    // For OpenGL
    using glm::value_ptr;
    using glm::min;
    using glm::max;

    using glm::cos;
    using glm::sin;
}

// Custom formatters for GLM types
template <>
struct std::formatter<glm::vec3> : std::formatter<std::string>
{
    auto format(const glm::vec3& v, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(
            std::format("({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z),
            ctx
        );
    }
};

template <>
struct std::formatter<glm::vec2> : std::formatter<std::string>
{
    auto format(const glm::vec2& v, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(
            std::format("({:.2f}, {:.2f})", v.x, v.y),
            ctx
        );
    }
};