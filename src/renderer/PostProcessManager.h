#pragma once
#include "renderer/interface/IFramebuffer.h"
#include "renderer/interface/IShader.h"
#include "renderer/interface/IRenderDevice.h"
#include "renderer/interface/IRenderer.h"
#include "renderer/opengl/GLScreenQuad.h"
#include "core/Window.h"
#include <memory>

/*
 * PostProcessManager.h
 *
 * PURPOSE:
 * Manages full-screen post-processing effects applied after 3D scene rendering. Handles
 * off-screen rendering, automatic framebuffer resizing, and effect application. Foundation
 * for bloom, tone mapping, color grading, and other screen-space effects. Uses interface
 * abstraction for API-agnostic rendering (November 6-15, 2025 refactors).
 *
 * DESIGN RATIONALE (October 26, 2025, Refactored November 6-15, 2025):
 * Problem: Post-processing effects (bloom, tone mapping, blur) need scene as texture.
 * Default framebuffer (screen) can't be read efficiently. Need HDR-capable rendering
 * (RGBA16F). Need automatic window resize handling (prevent crashes). Need API abstraction
 * (prepare for Vulkan).
 *
 * Solution: Off-screen rendering to HDR framebuffer with post-process pass.
 * - Framebuffer: RGBA16F color (HDR), DEPTH24_STENCIL8 depth
 * - Render-to-texture: 3D scene -> framebuffer -> post-process -> screen
 * - Automatic resize: Detect window changes, recreate framebuffer
 * - Interface abstraction: IFramebuffer, IShader, IRenderer (November 6-15)
 * - Result: Foundation for all screen-space effects
 *
 * Key Insight: Post-processing essential for modern rendering (bloom, tone mapping, AA).
 * Off-screen rendering enables texture-based effects. HDR framebuffer (RGBA16F) critical
 * for bloom (needs values > 1.0). Automatic resize prevents crashes/artifacts on window
 * changes. Interface abstraction (November 6-15) prepares for Vulkan.
 *
 * DESIGN PHILOSOPHY:
 * - Off-screen rendering: Scene to texture (not screen)
 * - HDR framebuffer: RGBA16F enables bloom/tone mapping
 * - Automatic resize: Seamless window size changes
 * - Interface abstraction: IFramebuffer, IShader, IRenderer (API-agnostic)
 * - Effect toggles: Runtime control (no shader recompilation)
 *
 * KEY CONCEPTS:
 * 1. Render-to-Texture:
 *    - Traditional: 3D Scene -> Screen
 *    - Post-process: 3D Scene -> Framebuffer (texture) -> Post-Process -> Screen
 *
 * 2. HDR Framebuffer:
 *    - RGBA16F: 16-bit float per channel (HDR values > 1.0)
 *    - Critical for bloom (bright values > 1.0)
 *    - Critical for tone mapping (HDR -> LDR)
 *
 * 3. Automatic Resize:
 *    - Detect: Compare window size to cached size
 *    - Recreate: Destroy old framebuffer, create new
 *    - Result: Seamless, no crashes/artifacts
 *
 * 4. Interface Abstraction (November 6-15):
 *    - IFramebuffer: API-agnostic framebuffer
 *    - IShader: API-agnostic shader
 *    - IRenderer: API-agnostic state management
 *    - Result: Same code works with OpenGL/Vulkan
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION ===
 * PostProcessManager postProcess(&window, renderDevice, renderer);
 *
 * // === FRAME LOOP ===
 * void render() {
 *     // 1. Begin post-processing (render to framebuffer)
 *     postProcess.beginScene();
 *
 *     // 2. Render 3D scene (writes to framebuffer texture)
 *     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 *     scene.render(camera, shader, window, *renderer);
 *
 *     // 3. End post-processing (apply effects, render to screen)
 *     postProcess.endScene();
 *     // Result: Scene with post-effects on screen
 * }
 *
 * // === EFFECT CONTROLS ===
 * // Enable grayscale (testing)
 * postProcess.setGrayscaleEnabled(true);
 *
 * // Disable grayscale (full color)
 * postProcess.setGrayscaleEnabled(false);
 *
 * // Query framebuffer size
 * int width = postProcess.getFramebufferWidth();
 * int height = postProcess.getFramebufferHeight();
 * ```
 *
 * RENDERING PIPELINE:
 *
 * ```cpp
 * void PostProcessManager::beginScene() {
 *     // Check for window resize
 *     int width = m_window->getWidth();
 *     int height = m_window->getHeight();
 *
 *     if (width != m_lastWidth || height != m_lastHeight) {
 *         // Window resized -> recreate framebuffer
 *         m_framebuffer = m_renderDevice->createFramebuffer(width, height);
 *         m_lastWidth = width;
 *         m_lastHeight = height;
 *         LOG_INFO("Framebuffer resized: {}x{}", width, height);
 *     }
 *
 *     // Bind framebuffer (redirect rendering to texture)
 *     m_framebuffer->bind();
 *
 *     // Set viewport to framebuffer size
 *     m_renderer->setViewport(0, 0, width, height);
 * }
 *
 * void PostProcessManager::endScene() {
 *     // Unbind framebuffer (return to screen)
 *     m_framebuffer->unbind();
 *
 *     // Setup for post-processing
 *     m_renderer->beginPostProcessPass();  // Depth OFF, blend OFF
 *
 *     // Bind post-process shader
 *     m_postProcessShader->bind();
 *
 *     // Set effect uniforms
 *     m_postProcessShader->setUniform("u_GrayscaleEnabled", m_grayscaleEnabled);
 *     m_postProcessShader->setUniform("u_ScreenTexture", 0);
 *
 *     // Bind scene texture
 *     m_framebuffer->getColorAttachment()->bind(0);
 *
 *     // Render full-screen quad (applies effect)
 *     m_screenQuad.render();
 *
 *     // End pass
 *     m_renderer->endPass();
 * }
 * ```
 *
 * AUTOMATIC RESIZE HANDLING:
 *
 * Problem: User resizes window mid-game
 * - Old framebuffer: 1280x720 (wrong size)
 * - New window: 1920x1080
 * - Result without resize: Stretched/artifacts/crashes
 *
 * Solution: Detect and recreate
 * ```cpp
 * // In beginScene()
 * int currentWidth = m_window->getWidth();
 * int currentHeight = m_window->getHeight();
 *
 * if (currentWidth != m_lastWidth || currentHeight != m_lastHeight) {
 *     // Recreate framebuffer at new size
 *     m_framebuffer = m_renderDevice->createFramebuffer(currentWidth, currentHeight);
 *     m_lastWidth = currentWidth;
 *     m_lastHeight = currentHeight;
 *     LOG_INFO("Framebuffer resized: {}x{}", currentWidth, currentHeight);
 * }
 * ```
 *
 * Result: Seamless resize, correct aspect ratio, no crashes
 *
 * EFFECT IMPLEMENTATION - Grayscale Example:
 *
 * Fragment shader (postprocess.frag):
 * ```glsl
 * #version 460 core
 *
 * in vec2 v_TexCoords;
 * out vec4 FragColor;
 *
 * uniform sampler2D u_ScreenTexture;  // Scene texture
 * uniform bool u_GrayscaleEnabled;    // Effect toggle
 *
 * void main() {
 *     // Sample scene texture
 *     vec3 color = texture(u_ScreenTexture, v_TexCoords).rgb;
 *
 *     if (u_GrayscaleEnabled) {
 *         // Convert to grayscale (luminance)
 *         float gray = dot(color, vec3(0.299, 0.587, 0.114));
 *         color = vec3(gray);
 *     }
 *
 *     FragColor = vec4(color, 1.0);
 * }
 * ```
 *
 * FRAMEBUFFER FORMAT:
 *
 * Color attachment (RGBA16F):
 * - Format: 16-bit float per channel (R, G, B, A)
 * - Range: [-65504, +65504] (HDR!)
 * - Memory: 8 bytes/pixel (1920x1080 = ~16MB)
 * - Use: Store scene with HDR values (bloom, tone mapping)
 *
 * Depth attachment (DEPTH24_STENCIL8):
 * - Format: 24-bit depth + 8-bit stencil (renderbuffer)
 * - Use: 3D rendering Z-buffer
 * - Not sampled: GPU-optimized (faster than texture)
 *
 * SCREEN QUAD - Full-Screen Rendering:
 *
 * Geometry:
 * - 4 vertices: (-1,-1), (1,-1), (1,1), (-1,1) (NDC coordinates)
 * - 2 triangles: (0,1,2), (2,3,0)
 * - UVs: (0,0), (1,0), (1,1), (0,1) (texture sampling)
 * - Result: Covers entire viewport
 *
 * Cost:
 * - Geometry: Negligible (4 verts, 6 indices)
 * - Fragment: 1 shader invocation per pixel (1920x1080 = 2M)
 * - Bottleneck: Fragment shader complexity (not geometry)
 *
 * INTERFACE ABSTRACTION EVOLUTION:
 *
 * October 26, 2025 (Initial - Direct OpenGL):
 * ```cpp
 * // Direct OpenGL calls
 * Framebuffer m_framebuffer;  // Concrete class
 * Shader m_shader;            // Concrete class
 *
 * glViewport(0, 0, width, height);
 * glDisable(GL_DEPTH_TEST);
 * glBindTexture(GL_TEXTURE_2D, m_framebuffer.getColorTexture());
 * ```
 * Problem: Hardcoded to OpenGL (Vulkan incompatible)
 *
 * November 6, 2025 (IShader + IFramebuffer):
 * ```cpp
 * // Interface-based
 * std::shared_ptr<IFramebuffer> m_framebuffer;  // Interface
 * std::shared_ptr<IShader> m_shader;            // Interface
 *
 * // Still some OpenGL calls
 * glViewport(0, 0, width, height);
 * glDisable(GL_DEPTH_TEST);
 * m_framebuffer->getColorAttachment()->bind(0);
 * ```
 * Progress: Framebuffer/shader abstracted, but state management not
 *
 * November 14, 2025 (IRenderer State Management):
 * ```cpp
 * // Full abstraction
 * std::shared_ptr<IFramebuffer> m_framebuffer;
 * std::shared_ptr<IShader> m_shader;
 * IRenderer* m_renderer;  // State management
 *
 * // No OpenGL calls
 * m_renderer->setViewport(0, 0, width, height);
 * m_renderer->beginPostProcessPass();  // Depth OFF, blend OFF
 * m_framebuffer->getColorAttachment()->bind(0);
 * ```
 * Result: Fully API-agnostic (GL/Vulkan ready)
 *
 * November 15, 2025 (GLScreenQuad):
 * ```cpp
 * // Screen quad now API-specific (GLScreenQuad, future VKScreenQuad)
 * GLScreenQuad m_screenQuad;  // OpenGL-specific (intentional)
 * ```
 * Rationale: Screen quad simple, duplicating cleaner than abstraction
 *
 * PERFORMANCE:
 *
 * Framebuffer creation (resize only):
 * - Cost: ~1-3ms (destroy old + create new)
 * - Frequency: Rare (only on window resize)
 *
 * beginScene() / endScene():
 * - Cost: ~0.01ms (bind/unbind framebuffer)
 * - Frequency: Every frame (negligible)
 *
 * Post-process pass:
 * - Grayscale: ~0.5ms (1080p)
 * - Bloom: ~2-5ms (depends on blur radius)
 * - Tone mapping: ~0.5ms
 * - Total typical: <2ms (acceptable at 60fps = 16.67ms budget)
 *
 * CURRENT STATE (November 15, 2025):
 * - Off-screen rendering to HDR framebuffer (RGBA16F)
 * - Automatic resize handling (seamless)
 * - Grayscale effect (test case, foundation)
 * - Full interface abstraction (IFramebuffer, IShader, IRenderer)
 * - GLScreenQuad (API-specific, intentional)
 * - Status: Production-ready foundation
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Effect:
 * - Only grayscale implemented (testing)
 * - Future: Bloom, tone mapping, DOF (Week 9-13)
 *
 * 2. No Effect Chaining:
 * - Single pass only (one shader)
 * - Future: Multi-pass (blur -> bloom -> tone map, Week 11-12)
 *
 * 3. No HDR Tone Mapping:
 * - RGBA16F framebuffer ready, but no tone map shader
 * - Future: ACES filmic (Week 9-10)
 *
 * 4. No Bloom:
 * - HDR framebuffer ready, but no bloom implementation
 * - Future: Bright extraction + Gaussian blur (Week 11-12)
 *
 * 5. GLScreenQuad (Not Abstracted):
 * - OpenGL-specific (intentional, simple to duplicate)
 * - Future: VKScreenQuad for Vulkan (separate class)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 26, 2025: Initial implementation
 * - Off-screen rendering to framebuffer
 * - Grayscale effect (test case)
 * - Direct OpenGL calls (no abstraction)
 *
 * November 6, 2025: IShader + IFramebuffer
 * - Replaced concrete Shader with IShader
 * - Replaced concrete Framebuffer with IFramebuffer
 * - Some OpenGL calls remain (state management)
 *
 * November 14, 2025: IRenderer State Management
 * - Removed direct OpenGL calls (glViewport, glDisable, etc.)
 * - Added IRenderer for state management
 * - renderer->beginPostProcessPass(), renderer->endPass()
 * - Result: Fully API-agnostic
 *
 * November 15, 2025: GLScreenQuad
 * - Changed ScreenQuad to GLScreenQuad (API-specific)
 * - Updated include: renderer/opengl/GLScreenQuad.h
 * - Intentional: Simple to duplicate, no abstraction overhead
 * - Status: Complete, production-ready foundation
 *
 * (FXAA):
 * - Fast anti-aliasing (post-process AA)
 * - Time: 1-2 days
 *
 * (Tone Mapping):
 * - ACES filmic (HDR -> LDR)
 * - Gamma correction (sRGB workflow)
 * - Dithering (prevent banding)
 * - Time: 2-3 days
 *
 * (Bloom):
 * - Bright extraction (threshold > 1.0)
 * - Gaussian blur (5 passes, downsampled)
 * - Additive blend with scene
 * - Time: 2-3 days
 *
 * (Depth of Field):
 * - Circle of confusion blur
 * - Bokeh effect (hexagonal kernel)
 * - Half-res rendering (optimization)
 * - Time: 5-7 days
 *
 * DEPENDENCIES:
 * - renderer/interface/IFramebuffer.h: Framebuffer abstraction
 * - renderer/interface/IShader.h: Shader abstraction
 * - renderer/interface/IRenderDevice.h: Factory for framebuffer creation
 * - renderer/interface/IRenderer.h: State management (November 14)
 * - renderer/opengl/GLScreenQuad.h: Full-screen quad (November 15)
 * - core/Window.h: Window size queries (resize detection)
 * - <memory>: std::shared_ptr
 *
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL context requirement
 * - All operations on main render thread only
 *
 * REFERENCES:
 * - LearnOpenGL.com: Framebuffers tutorial
 * - Real-Time Rendering 4th Ed., Chapter 12: Image-space effects
 * - GPU Gems: Post-processing techniques
 *
 * HISTORY:
 * October 26, 2025: Initial implementation
 * - Off-screen rendering to framebuffer (RGBA16F)
 * - Automatic resize detection and handling
 * - Grayscale effect (test case)
 * - Direct OpenGL state management (not abstracted)
 *
 * November 6, 2025: Interface abstraction (Phase 1)
 * - Replaced Shader with IShader interface
 * - Replaced Framebuffer with IFramebuffer interface
 * - Framebuffer created via renderDevice->createFramebuffer()
 * - Some OpenGL calls remain (glViewport, glDisable)
 *
 * November 14, 2025: State management abstraction (Phase 2)
 * - Removed direct OpenGL calls (glViewport, glDisable, glDepthMask)
 * - Added IRenderer for state management
 * - renderer->setViewport(), renderer->beginPostProcessPass()
 * - Result: Fully API-agnostic (GL/Vulkan ready)
 *
 * November 15, 2025: GLScreenQuad (API-specific by design)
 * - Changed ScreenQuad to GLScreenQuad (OpenGL-specific)
 * - Updated include and member variable
 * - Intentional: Screen quad simple, no abstraction needed
 * - Future: VKScreenQuad for Vulkan (separate class)
 *
 */

namespace Engine
{
    class PostProcessManager
    {
    public:
        // Constructor: Takes window reference and render device
        PostProcessManager(Window* window, IRenderDevice* renderDevice, IRenderer* renderer);
        ~PostProcessManager() = default;

        // Begin post-process pass (bind framebuffer, redirect rendering to texture)
        void beginScene();

        // End post-process pass (apply effects, render final result to screen)
        void endScene();

        // Effect controls
        void setGrayscaleEnabled(bool enabled) { m_grayscaleEnabled = enabled; }
        bool isGrayscaleEnabled() const { return m_grayscaleEnabled; }

        // Query framebuffer dimensions
        int getFramebufferWidth() const;
        int getFramebufferHeight() const;

    private:
        // Window reference (for dynamic sizing)
        Window* m_window;

        // Render device (for creating framebuffers)
        IRenderDevice* m_renderDevice;

        IRenderer* m_renderer;

        // Post-processing components
        std::shared_ptr<IFramebuffer> m_framebuffer;  // Off-screen render target
        GLScreenQuad m_screenQuad;                      // Full-screen quad mesh
        std::shared_ptr<IShader> m_postProcessShader; // Post-process shader

        // Effect toggles
        bool m_grayscaleEnabled = true;  // Convert to grayscale (testing)

        // Resize detection (track last known window size)
        int m_lastWidth = 0;
        int m_lastHeight = 0;
    };
}