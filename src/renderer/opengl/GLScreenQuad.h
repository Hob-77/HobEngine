#pragma once
#include <glad/glad.h>

/*
 * GLScreenQuad.h
 *
 * PURPOSE:
 * OpenGL-specific full-screen quad for post-processing and texture display. Two triangles
 * covering entire viewport in NDC coordinates (no transformations needed). Essential for
 * screen-space effects (bloom, blur, tone mapping, SSAO, SSR). Simple, efficient, reusable.
 * No interface abstraction - API-specific by design (Vulkan will have VKScreenQuad).
 *
 * DESIGN RATIONALE (November 15, 2025):
 * Problem: Post-processing effects need to render full-screen with texture sampling. Need
 * simple geometry covering entire viewport. Traditional 3D meshes overkill (no transforms,
 * no lighting). Need efficient, reusable solution for all screen-space effects.
 *
 * Solution: Hardcoded NDC quad ([-1,1] position, [0,1] UV).
 * - NDC coordinates: No MVP matrix needed (already screen-space)
 * - Two triangles: 4 vertices, 6 indices (standard)
 * - Stateless: No per-render configuration
 * - Reusable: Same quad for all post-process effects
 * - API-specific: OpenGL VAO/VBO/EBO (Vulkan uses VkBuffer)
 *
 * Key Insight: Screen quad is API-specific by nature. OpenGL uses VAO/VBO/EBO, Vulkan
 * uses VkBuffer/VkDeviceMemory. Interface abstraction unnecessary (simple, duplicating
 * code cleaner than abstraction overhead). Future VKScreenQuad will be separate class.
 *
 * DESIGN PHILOSOPHY:
 * - Simplicity: Hardcoded geometry, no configuration
 * - Efficiency: NDC coordinates (no transforms)
 * - Reusability: One instance for all effects
 * - API-specific: No interface (intentional)
 * - Stateless: Assumes shader already bound
 *
 * KEY CONCEPTS:
 * 1. NDC Coordinates (Normalized Device Coordinates):
 *    - Position range: [-1, 1] for X and Y
 *    - Covers entire viewport (no transformation needed)
 *    - Result: Two triangles filling screen
 *
 * 2. UV Coordinates:
 *    - Range: [0, 1] for U and V
 *    - Maps to entire texture
 *    - (0,0) = bottom-left, (1,1) = top-right
 *
 * 3. Two Triangles:
 *    - Triangle 1: (0,1,2) top-left half
 *    - Triangle 2: (2,3,0) bottom-right half
 *    - Result: Full-screen quad
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION (Once) ===
 * GLScreenQuad screenQuad;  // Creates VAO/VBO/EBO
 *
 * // === POST-PROCESSING (Every Frame) ===
 * // 1. Render scene to framebuffer
 * framebuffer->bind();
 * scene.render(camera, shader, window, renderer);
 * framebuffer->unbind();
 *
 * // 2. Apply post-processing effect
 * postProcessShader->bind();
 * postProcessShader->setUniform("u_ScreenTexture", 0);
 *
 * // 3. Bind scene texture
 * framebuffer->getColorAttachment()->bind(0);
 *
 * // 4. Render full-screen quad (applies effect)
 * screenQuad.render();
 *
 * // === MULTIPLE EFFECTS (Chaining) ===
 * // Grayscale
 * grayscaleShader->bind();
 * framebuffer1->getColorAttachment()->bind(0);
 * screenQuad.render();  // Render to framebuffer2
 *
 * // Blur
 * blurShader->bind();
 * framebuffer2->getColorAttachment()->bind(0);
 * screenQuad.render();  // Render to framebuffer3
 *
 * // Tone mapping (final, to screen)
 * toneMappingShader->bind();
 * framebuffer3->getColorAttachment()->bind(0);
 * screenQuad.render();  // Render to screen
 * ```
 *
 * GEOMETRY DEFINITION:
 *
 * Vertices (4 total):
 * ```cpp
 * float vertices[] = {
 *     // Position (NDC)   UV coords
 *     -1.0f, -1.0f,      0.0f, 0.0f,  // Bottom-left
 *      1.0f, -1.0f,      1.0f, 0.0f,  // Bottom-right
 *      1.0f,  1.0f,      1.0f, 1.0f,  // Top-right
 *     -1.0f,  1.0f,      0.0f, 1.0f   // Top-left
 * };
 * // 4 vertices × 4 floats = 16 floats = 64 bytes
 * ```
 *
 * Indices (6 total, 2 triangles):
 * ```cpp
 * unsigned int indices[] = {
 *     0, 1, 2,  // Triangle 1 (bottom-left half)
 *     2, 3, 0   // Triangle 2 (top-right half)
 * };
 * // 6 indices × 4 bytes = 24 bytes
 * ```
 *
 * Total memory: 64 + 24 = 88 bytes (trivial)
 *
 * OPENGL SETUP:
 *
 * ```cpp
 * GLScreenQuad::GLScreenQuad() {
 *     // Define geometry
 *     float vertices[] = {
 *         // pos          uv
 *         -1.0f, -1.0f,  0.0f, 0.0f,
 *          1.0f, -1.0f,  1.0f, 0.0f,
 *          1.0f,  1.0f,  1.0f, 1.0f,
 *         -1.0f,  1.0f,  0.0f, 1.0f
 *     };
 *
 *     unsigned int indices[] = {
 *         0, 1, 2,  2, 3, 0
 *     };
 *
 *     // Create VAO
 *     glGenVertexArrays(1, &m_vao);
 *     glBindVertexArray(m_vao);
 *
 *     // Create VBO (vertex data)
 *     glGenBuffers(1, &m_vbo);
 *     glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
 *     glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
 *
 *     // Create EBO (index data)
 *     glGenBuffers(1, &m_ebo);
 *     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
 *     glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
 *
 *     // Setup vertex attributes
 *     // Position (location 0): 2 floats at offset 0
 *     glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
 *     glEnableVertexAttribArray(0);
 *
 *     // UV (location 1): 2 floats at offset 2
 *     glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
 *                           (void*)(2 * sizeof(float)));
 *     glEnableVertexAttribArray(1);
 *
 *     // Unbind (clean state)
 *     glBindVertexArray(0);
 * }
 * ```
 *
 * RENDERING:
 *
 * ```cpp
 * void GLScreenQuad::render() const {
 *     glBindVertexArray(m_vao);
 *     glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
 *     glBindVertexArray(0);
 * }
 * ```
 *
 * Cost: ~10-20 cycles (negligible compared to fragment shader)
 *
 * SHADER COMPATIBILITY:
 *
 * Vertex shader:
 * ```glsl
 * #version 460 core
 *
 * layout(location = 0) in vec2 a_Position;  // NDC coords
 * layout(location = 1) in vec2 a_TexCoords; // UV coords
 *
 * out vec2 v_TexCoords;
 *
 * void main() {
 *     v_TexCoords = a_TexCoords;
 *     gl_Position = vec4(a_Position, 0.0, 1.0);  // No transformation
 * }
 * ```
 *
 * Fragment shader (grayscale example):
 * ```glsl
 * #version 460 core
 *
 * in vec2 v_TexCoords;
 * out vec4 FragColor;
 *
 * uniform sampler2D u_ScreenTexture;
 *
 * void main() {
 *     vec3 color = texture(u_ScreenTexture, v_TexCoords).rgb;
 *     float gray = dot(color, vec3(0.299, 0.587, 0.114));
 *     FragColor = vec4(vec3(gray), 1.0);
 * }
 * ```
 *
 * PERFORMANCE ANALYSIS:
 *
 * Geometry cost (negligible):
 * - 4 vertices, 6 indices (88 bytes total)
 * - 1 draw call (glDrawElements)
 * - Vertex shader: 4 invocations (~0.001ms)
 *
 * Fragment cost (bottleneck):
 * - 1080p: 1920 × 1080 = 2,073,600 fragments
 * - Simple shader (grayscale): ~0.5ms
 * - Complex shader (bloom): ~2-5ms
 * - Bottleneck: Fragment shader complexity
 *
 * Result: Screen quad geometry is FREE (fragment shader dominates cost)
 *
 * USE CASES:
 *
 * Post-processing effects:
 * - Grayscale, sepia, color grading
 * - Bloom, blur (Gaussian, box, radial)
 * - Tone mapping (ACES, Reinhard, Uncharted)
 * - Vignette, chromatic aberration, film grain
 *
 * Screen-space effects:
 * - SSAO (screen-space ambient occlusion)
 * - SSR (screen-space reflections)
 * - God rays, volumetric lighting
 * - Motion blur, depth of field
 *
 * Texture display:
 * - Show framebuffer contents (debug)
 * - UI overlays, fades, transitions
 * - Render targets to screen
 *
 * CURRENT STATE (November 15, 2025):
 * - OpenGL implementation (VAO/VBO/EBO)
 * - NDC coordinates (no transformations)
 * - Indexed rendering (6 indices, 2 triangles)
 * - RAII resource management, move-only semantics
 *
 * NO INTERFACE ABSTRACTION:
 * - API-specific by design (OpenGL VAO/VBO vs Vulkan VkBuffer)
 * - Simple implementation (abstraction unnecessary)
 * - Future: VKScreenQuad will be separate class
 * - Rationale: Clean duplication better than abstraction overhead
 *
 * FUTURE (Vulkan Renderer):
 * - VKScreenQuad class (separate, no shared interface)
 * - VkBuffer for vertex/index data
 * - VkPipeline for rendering
 * - Same geometry, different API
 * - Time: 1-2 hours (simple port)
 *
 * DEPENDENCIES:
 * - <glad/glad.h>: OpenGL function loader
 *
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL context requirement
 * - Construction: Main thread only (glGenVertexArrays, glBufferData)
 * - Rendering: Main thread only (glDrawElements)
 * - Move operations: Thread-safe (no OpenGL calls)
 *
 * REFERENCES:
 * - LearnOpenGL.com: Framebuffers tutorial (screen quad example)
 * - OpenGL 4.6 Specification: Vertex arrays, buffer objects
 *
 * HISTORY:
 * October 26, 2025: Original implementation
 * - Same as November 15, 2025 implementation
 * - Method was called ScreenQuad
 * - This was just a hardcoded OpenGL ScreenQuad and name change was planned
 * November 15, 2025: Initial implementation
 * - OpenGL screen quad (VAO/VBO/EBO)
 * - NDC coordinates (no transformations)
 * - Indexed rendering (2 triangles, 6 indices)
 * - RAII resource management, move-only semantics
 * - Result: Essential post-processing primitive
 *
 */

namespace Engine
{
    class GLScreenQuad
    {
    public:
        GLScreenQuad();
        ~GLScreenQuad();

        // No copying (OpenGL resources)
        GLScreenQuad(const GLScreenQuad&) = delete;
        GLScreenQuad& operator=(const GLScreenQuad&) = delete;

        // Move semantics
        GLScreenQuad(GLScreenQuad&& other) noexcept;
        GLScreenQuad& operator=(GLScreenQuad&& other) noexcept;

        // Render the quad (assumes shader is already bound)
        void render() const;

    private:
        GLuint m_vao = 0;  // Vertex Array Object
        GLuint m_vbo = 0;  // Vertex Buffer Object
        GLuint m_ebo = 0;  // Element Buffer Object
    };
}