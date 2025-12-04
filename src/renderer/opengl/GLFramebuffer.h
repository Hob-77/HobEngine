#pragma once
#include "renderer/interface/IFramebuffer.h"
#include <glad/glad.h>

/*
 * GLFramebuffer.h
 *
 * PURPOSE:
 * OpenGL framebuffer object (FBO) implementation for off-screen rendering. Enables rendering
 * 3D scenes to textures instead of screen. Foundation for post-processing (bloom, blur, tone
 * mapping). Implements IFramebuffer interface for renderer abstraction. Uses RGBA16F color
 * attachment for HDR support (values > 1.0). Managed by PostProcessManager for automatic
 * resize and lifecycle.
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Post-processing effects (grayscale, bloom, blur) need scene as texture. Default
 * framebuffer (screen) can't be read efficiently. Need HDR-capable color buffer (RGBA8
 * insufficient, clamps values to 1.0). Need depth buffer for 3D rendering.
 *
 * Solution: Custom FBO with HDR color attachment and depth renderbuffer.
 * - Color: RGBA16F (16-bit float per channel, HDR values > 1.0)
 * - Depth: DEPTH24_STENCIL8 renderbuffer (24-bit depth + 8-bit stencil)
 * - RAII: Constructor creates, destructor deletes
 * - Resize: Destroy old, create new (handles window resize)
 * - Validation: Check GL_FRAMEBUFFER_COMPLETE, log errors
 *
 * Key Insight: RGBA16F critical for HDR pipeline. Bloom needs bright values > 1.0 (sun,
 * explosions). RGBA8 clamps to 1.0 (no bloom possible). Industry standard (Unreal, Unity,
 * id Tech) uses RGBA16F. Memory cost acceptable (~16MB for 1080p).
 *
 * DESIGN PHILOSOPHY:
 * - HDR by default: RGBA16F enables future effects (bloom, tone mapping)
 * - RAII: Constructor creates, destructor deletes GPU resources
 * - Move-only: Prevent GPU resource duplication
 * - Validation: Check completeness, log detailed errors
 * - Abstraction: Return ITexture interfaces (not raw GL textures)
 *
 * KEY CONCEPTS:
 * 1. Framebuffer Rendering:
 *    - Default: 3D Scene -> Screen (FBO ID 0)
 *    - Custom: 3D Scene -> FBO Texture -> Post-Process -> Screen
 *    - Result: Apply effects (grayscale, bloom, blur) to scene
 *
 * 2. Color Attachment (RGBA16F):
 *    - Format: 16-bit float per channel (R, G, B, A)
 *    - Range: [-65504, +65504] (supports HDR!)
 *    - Memory: 8 bytes/pixel (1920×1080 = ~16MB)
 *    - Use: Store rendered scene with HDR values
 *
 * 3. Depth Attachment (DEPTH24_STENCIL8):
 *    - Format: 24-bit depth + 8-bit stencil (renderbuffer, not texture)
 *    - Use: 3D rendering Z-buffer (depth testing)
 *    - Performance: Renderbuffer faster than texture (GPU-optimized)
 *    - Limitation: Can't sample in shader (no SSAO/fog yet)
 *
 * 4. Framebuffer Validation:
 *    - GL_FRAMEBUFFER_COMPLETE: Ready to use
 *    - GL_FRAMEBUFFER_INCOMPLETE_*: Something wrong (log error)
 *    - Checked after creation, logs detailed message
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === VIA POSTPROCESSMANAGER (Recommended) ===
 * PostProcessManager postProcess(window, renderDevice);
 *
 * // Begin scene (binds FBO)
 * postProcess.beginScene();
 *
 * // Render 3D scene to texture
 * glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 * scene.render(camera, shader, window, renderer);
 *
 * // End scene (unbinds FBO, applies effects, renders to screen)
 * postProcess.endScene();
 * // Result: Grayscale scene on screen
 *
 * // === MANUAL USAGE (Low-Level) ===
 * auto fbo = renderDevice->createFramebuffer(1920, 1080);
 *
 * // Render to FBO
 * fbo->bind();
 * glViewport(0, 0, 1920, 1080);
 * glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 * scene.render(camera, shader, window, renderer);
 * fbo->unbind();
 *
 * // Apply post-processing
 * postProcessShader->bind();
 * postProcessShader->setUniform("u_SceneTexture", 0);
 * fbo->getColorAttachment()->bind(0);  // Bind scene texture
 * screenQuad->draw();  // Full-screen quad with effect
 *
 * // === RESIZE HANDLING (Automatic via PostProcessManager) ===
 * // Window resized -> PostProcessManager detects in beginScene()
 * // -> Calls fbo->resize(newWidth, newHeight)
 * // -> Old FBO destroyed, new created (seamless)
 * ```
 *
 * RGBA16F - Why HDR Color Format:
 *
 * RGBA8 (standard 8-bit):
 * - Range: [0, 1] per channel (256 levels)
 * - Problem: Bright values clamped (sun, explosions, bloom)
 * - Problem: Color banding in gradients (only 256 levels)
 * - Size: 4 bytes/pixel (1920×1080 = ~8MB)
 *
 * RGBA16F (half-precision float):
 * - Range: [-65504, +65504] per channel (HDR!)
 * - Benefit: Bright values preserved (sun = 10.0, explosions = 5.0)
 * - Benefit: Smooth gradients (no banding)
 * - Size: 8 bytes/pixel (1920×1080 = ~16MB)
 * - Cost: 2× memory, but HDR essential for modern rendering
 *
 * HDR pipeline example:
 * ```
 * Sun (10.0) -> Bloom (extract > 1.0) -> Blur -> Add to scene -> Tone map -> Screen (0-1)
 * ```
 *
 * Without HDR (RGBA8):
 * - Sun clamped to 1.0 -> No bloom (can't extract bright values)
 *
 * With HDR (RGBA16F):
 * - Sun = 10.0 -> Bloom extracts (10.0 - 1.0 = 9.0) -> Beautiful glow
 *
 * FRAMEBUFFER CONSTRUCTION:
 *
 * ```cpp
 * GLFramebuffer::GLFramebuffer(int width, int height, IRenderDevice* renderDevice)
 *     : m_width(width)
 *     , m_height(height)
 *     , m_renderDevice(renderDevice)
 * {
 *     create();  // Create FBO and attachments
 *     createTextureWrappers();  // Wrap GL textures in ITexture interfaces
 * }
 *
 * void GLFramebuffer::create() {
 *     // 1. Create framebuffer object
 *     glGenFramebuffers(1, &m_fbo);
 *     glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
 *
 *     // 2. Create color attachment (RGBA16F texture)
 *     glGenTextures(1, &m_colorTexture);
 *     glBindTexture(GL_TEXTURE_2D, m_colorTexture);
 *     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
 *                  GL_RGBA, GL_FLOAT, nullptr);
 *
 *     // Filtering (linear for smooth post-processing)
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 *
 *     // Clamping (prevent edge artifacts)
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 *
 *     // Attach to FBO
 *     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
 *                            GL_TEXTURE_2D, m_colorTexture, 0);
 *
 *     // 3. Create depth/stencil renderbuffer (DEPTH24_STENCIL8)
 *     glGenRenderbuffers(1, &m_depthRBO);
 *     glBindRenderbuffer(GL_RENDERBUFFER, m_depthRBO);
 *     glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
 *                           m_width, m_height);
 *
 *     // Attach to FBO
 *     glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
 *                               GL_RENDERBUFFER, m_depthRBO);
 *
 *     // 4. Validate completeness
 *     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
 *     if (status == GL_FRAMEBUFFER_COMPLETE) {
 *         m_isComplete = true;
 *     } else {
 *         LOG_ERROR("Framebuffer incomplete: {}", getStatusString(status));
 *         m_isComplete = false;
 *     }
 *
 *     // 5. Unbind (clean state)
 *     glBindFramebuffer(GL_FRAMEBUFFER, 0);
 * }
 *
 * void GLFramebuffer::createTextureWrappers() {
 *     // Wrap GL textures in ITexture interfaces (abstraction layer)
 *     m_colorAttachment = std::make_shared<GLTextureView>(
 *         m_colorTexture, m_width, m_height, 4);  // RGBA = 4 channels
 *
 *     // Depth not wrapped yet (renderbuffer, not texture)
 *     m_depthAttachment = nullptr;
 * }
 * ```
 *
 * VALIDATION ERRORS:
 *
 * ```cpp
 * const char* getStatusString(GLenum status) {
 *     switch (status) {
 *         case GL_FRAMEBUFFER_COMPLETE:
 *             return "Complete";
 *         case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
 *             return "Incomplete attachment (invalid format or size)";
 *         case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
 *             return "Missing attachment (no color or depth)";
 *         case GL_FRAMEBUFFER_UNSUPPORTED:
 *             return "Unsupported format combination";
 *         case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
 *             return "Incomplete draw buffer";
 *         case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
 *             return "Incomplete read buffer";
 *         default:
 *             return "Unknown error";
 *     }
 * }
 * ```
 *
 * RESIZE HANDLING:
 *
 * ```cpp
 * void GLFramebuffer::resize(int width, int height) {
 *     if (width == m_width && height == m_height) {
 *         return;  // No change, skip
 *     }
 *
 *     // Update dimensions
 *     m_width = width;
 *     m_height = height;
 *
 *     // Destroy old FBO and attachments
 *     destroy();
 *
 *     // Create new FBO at new size
 *     create();
 *
 *     // Re-create texture wrappers
 *     createTextureWrappers();
 * }
 *
 * void GLFramebuffer::destroy() {
 *     if (m_fbo != 0) {
 *         glDeleteFramebuffers(1, &m_fbo);
 *         m_fbo = 0;
 *     }
 *     if (m_colorTexture != 0) {
 *         glDeleteTextures(1, &m_colorTexture);
 *         m_colorTexture = 0;
 *     }
 *     if (m_depthRBO != 0) {
 *         glDeleteRenderbuffers(1, &m_depthRBO);
 *         m_depthRBO = 0;
 *     }
 * }
 * ```
 *
 * POST-PROCESSING INTEGRATION:
 *
 * Rendering flow:
 * ```
 * 1. fbo->bind() -> Redirect rendering to texture
 * 2. glClear() -> Clear color + depth
 * 3. scene.render() -> Draw 3D objects to FBO
 * 4. fbo->unbind() -> Return to screen (FBO ID 0)
 * 5. postProcessShader->bind() -> Activate effect (grayscale, etc.)
 * 6. fbo->getColorAttachment()->bind(0) -> Bind scene texture to slot 0
 * 7. screenQuad->draw() -> Full-screen quad with effect applied
 * 8. Result: Scene with effect on screen
 * ```
 *
 * Effect chaining:
 * ```
 * Scene -> FBO1 (scene texture)
 *      -> Extract bright (> 1.0) -> FBO2 (bright texture)
 *      -> Gaussian blur -> FBO3 (blurred bright)
 *      -> Combine (scene + blurred) -> FBO4 (bloom result)
 *      -> Tone map (HDR -> LDR) -> Screen
 * ```
 *
 * MEMORY USAGE:
 *
 * 1920×1080 framebuffer:
 * - Color: 1920 × 1080 × 8 bytes = 16,588,800 bytes (~16MB)
 * - Depth: 1920 × 1080 × 4 bytes = 8,294,400 bytes (~8MB)
 * - Total: ~24MB GPU memory
 *
 * Multiple FBOs (effect chaining):
 * - 4 FBOs × 24MB = ~96MB GPU memory (acceptable)
 * - Half-resolution for blur: 960×540 = ~6MB (optimization)
 *
 * PERFORMANCE:
 * - bind()/unbind(): ~0.01ms (state change only)
 * - resize(): ~1-3ms (destroy + recreate attachments)
 * - Render to FBO: Same as screen (no overhead)
 * - Post-process pass: ~0.5-1ms per effect (grayscale, blur)
 * - Total overhead: <2ms (60fps = 16.67ms budget, plenty of headroom)
 *
 * CURRENT STATE (November 6, 2025):
 * - RGBA16F color attachment (HDR support)
 * - DEPTH24_STENCIL8 renderbuffer (depth testing)
 * - Automatic resize handling
 * - Framebuffer validation (detailed errors)
 * - ITexture abstraction (getColorAttachment)
 * - RAII resource management, move-only semantics
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Color Attachment:
 * - Only one RGBA16F texture (no MRT)
 * - Can't render multiple properties simultaneously
 * - Future: Multiple attachments for deferred rendering 
 *
 * 2. Depth as Renderbuffer:
 * - Can't sample depth in shader (no SSAO/fog/DOF)
 * - Renderbuffer = fast but read-only
 * - Future: Depth texture for screen-space effects
 *
 * 3. No MSAA Support:
 * - No multisampling (jagged edges in FBO)
 * - Future: Multisample FBO + resolve pass
 *
 * 4. No Mipmaps:
 * - Color attachment has no mipmaps (needed for bloom downsampling)
 * - Future: Generate mipmaps for bloom chain
 *
 * 5. Stencil Allocated But Unused:
 * - DEPTH24_STENCIL8 includes stencil, but not used yet
 * - Future: Stencil masking for effects
 *
 * INTEGRATION WITH ROADMAP:
 *
 * November 6, 2025: Initial implementation
 * - OpenGL FBO wrapper (glGenFramebuffers, glFramebufferTexture2D)
 * - RGBA16F color attachment (HDR support)
 * - DEPTH24_STENCIL8 renderbuffer (depth testing)
 * - Validation and error handling
 * - RAII resource management, move-only semantics
 * - IFramebuffer interface implementation
 *
 * (Bloom Implementation):
 * - Mipmap generation on color attachment
 * - Multiple FBOs for effect chain
 * - Time: 2-3 days
 *
 * (SSAO/Fog):
 * - Depth as texture (not renderbuffer)
 * - Shader sampling of depth buffer
 * - Time: 1 day
 *
 * (MSAA Support):
 * - Multisample FBO (GL_TEXTURE_2D_MULTISAMPLE)
 * - Resolve pass to non-MSAA texture
 * - Time: 2-3 days
 *
 * (Deferred Rendering):
 * - Multiple color attachments (MRT)
 * - G-buffer (position, normal, albedo, specular)
 * - Time: 1 week
 *
 * DEPENDENCIES:
 * - renderer/interface/IFramebuffer.h: Abstract interface
 * - renderer/interface/IRenderDevice.h: Factory for texture wrappers
 * - <glad/glad.h>: OpenGL function loader
 *
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL context requirement
 * - Creation: Main thread only (glGenFramebuffers, glTexImage2D)
 * - Binding: Main thread only (glBindFramebuffer)
 * - Resize: Main thread only (destroy + recreate)
 *
 * REFERENCES:
 * - OpenGL 4.6 Specification: Framebuffer objects
 * - LearnOpenGL.com: Framebuffers tutorial
 * - Real-Time Rendering 4th Ed., Chapter 23: Graphics hardware (FBO details)
 * - IFramebuffer.h: Interface documentation
 *
 * HISTORY:
 * October 26, 2025: Original implementation
 * - Framebuffer was intially standalone class that was OpenGL with no interface for testing
 * - Did the same job as the interface implementation except we renamed
 * - and structured class for interface
 * 
 * November 6, 2025: Initial implementation
 * - OpenGL FBO wrapper (glGenFramebuffers)
 * - RGBA16F color attachment (HDR support for bloom/tone mapping)
 * - DEPTH24_STENCIL8 renderbuffer (depth testing)
 * - Framebuffer validation (GL_FRAMEBUFFER_COMPLETE check)
 * - Resize support (destroy + recreate)
 * - ITexture abstraction (GLTextureView wrappers)
 * - RAII resource management, move-only semantics
 * - Result: HDR-capable off-screen rendering
 *
 */

namespace Engine
{
    // Forward declare for factory method
    class IRenderDevice;

    class GLFramebuffer : public IFramebuffer
    {
    public:
        // Constructor - now takes render device for creating texture wrappers
        GLFramebuffer(int width, int height, IRenderDevice* renderDevice);

        ~GLFramebuffer() override;

        // Move semantics (no copying)
        GLFramebuffer(GLFramebuffer&& other) noexcept;
        GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;
        GLFramebuffer(const GLFramebuffer&) = delete;
        GLFramebuffer& operator=(const GLFramebuffer&) = delete;

        // IFramebuffer interface implementation
        void bind() const override;
        void unbind() const override;
        void resize(int width, int height) override;

        int getWidth() const override { return m_width; }
        int getHeight() const override { return m_height; }
        bool isComplete() const override { return m_isComplete; }

        // UPDATED: Return ITexture interfaces (maintains abstraction)
        std::shared_ptr<ITexture> getColorAttachment() const override { return m_colorAttachment; }
        std::shared_ptr<ITexture> getDepthAttachment() const override { return m_depthAttachment; }

        // OpenGL-specific queries (not in interface, for internal use only)
        GLuint getID() const { return m_fbo; }
        GLuint getColorTextureID() const { return m_colorTexture; }  // Direct GL access if needed
        GLuint getDepthRBO() const { return m_depthRBO; }

    private:
        void create();   // Create FBO and attachments
        void destroy();  // Delete all OpenGL objects
        void createTextureWrappers();  // Wrap GL textures in ITexture interfaces

    private:
        GLuint m_fbo = 0;            // Framebuffer object
        GLuint m_colorTexture = 0;   // Color attachment (RGBA16F)
        GLuint m_depthRBO = 0;       // Depth renderbuffer (DEPTH24_STENCIL8)

        int m_width = 0;
        int m_height = 0;
        bool m_isComplete = false;

        // NEW: ITexture wrappers for abstraction layer
        std::shared_ptr<ITexture> m_colorAttachment;
        std::shared_ptr<ITexture> m_depthAttachment;
        IRenderDevice* m_renderDevice = nullptr;  // Non-owning pointer
    };

}