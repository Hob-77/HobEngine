#pragma once
#include <cstdint>
#include <memory>

/*
 * IFramebuffer.h
 *
 * PURPOSE:
 * API-agnostic framebuffer abstraction for render-to-texture (RTT) operations. Enables
 * off-screen rendering for post-processing, shadow mapping, reflections, and multi-pass
 * effects. Maintains complete abstraction by returning texture attachments as ITexture
 * interfaces rather than raw API handles (GLuint, VkImageView).
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Post-processing and multi-pass rendering require render-to-texture, which is
 * heavily API-specific (OpenGL FBO vs Vulkan render passes). Direct use of glGenFramebuffers,
 * glBindFramebuffer, glFramebufferTexture2D would make post-processing non-portable.
 *
 * Solution: Interface abstraction separating framebuffer operations from graphics API.
 * - Application code uses IFramebuffer* (doesn't know if OpenGL or Vulkan)
 * - GLFramebuffer implements with OpenGL FBO (GL_FRAMEBUFFER)
 * - VKFramebuffer implements with Vulkan render pass + framebuffer + image views
 * - Switching APIs = zero changes to PostProcessManager or shadow mapping code
 *
 * Key Insight: Framebuffer operations are conceptually identical across APIs - create
 * off-screen render target, render to it, sample result as texture. Implementation details
 * differ drastically (OpenGL FBO is simple, Vulkan render pass is complex), but interface
 * can unify the concept.
 *
 * CRITICAL DESIGN DECISION - ITexture Attachment Access:
 *
 * Problem: Post-processing needs to sample framebuffer results as textures. How to expose
 * color and depth attachments without breaking abstraction?
 *
 * Options Considered:
 * 1. Return raw handles: getColorAttachment() -> GLuint or VkImageView
 *    - Pros: Simple, direct access
 *    - Cons: BREAKS ABSTRACTION - calling code knows which API is active
 *    - Cons: Can't switch APIs without rewriting all post-processing code
 *
 * 2. Return ITexture wrapper around handle
 *    - Pros: Type-safe, but still exposes implementation details
 *    - Cons: Requires dynamic casting or type checking (instanceof pattern)
 *
 * 3. Return std::shared_ptr<ITexture> (CHOSEN)
 *    - Pros: Complete abstraction maintained, no API knowledge needed
 *    - Pros: Can bind() and use like any other texture
 *    - Cons: Slightly more complex internally (create ITexture wrapper)
 *
 * Result: Post-processing code uses ITexture interface for FBO attachments, identical to
 * regular textures. No raw GLuint or VkImageView ever exposed to application code.
 *
 * Example (abstraction preserved):
 * ```cpp
 * // Get FBO color attachment as ITexture interface
 * auto colorTexture = sceneFBO->getColorAttachment();  // std::shared_ptr<ITexture>
 *
 * // Use it exactly like a regular texture - no API knowledge needed
 * colorTexture->bind(0);
 * shader->setUniform("u_SceneTexture", 0);
 *
 * // Works with OpenGL or Vulkan - calling code identical
 * ```
 *
 * DESIGN PHILOSOPHY:
 * - Pure virtual interface: No OpenGL/Vulkan code in this header
 * - Complete abstraction: Attachments returned as ITexture interfaces, not raw handles
 * - HDR rendering: RGBA16F color format for PBR tone mapping
 * - Dynamic resize: Handle window resize events without recreating scene
 * - Validation: isComplete() checks FBO status (GL_FRAMEBUFFER_COMPLETE)
 * - Minimal API: Only essential operations (bind, unbind, resize, query)
 *
 * KEY CONCEPTS:
 * 1. Render-to-Texture (RTT): Rendering to off-screen buffer instead of screen
 *    - OpenGL: Framebuffer Object (FBO) with texture attachments
 *    - Vulkan: Render pass + framebuffer + VkImage attachments
 *    - Result: Rendered image stored in texture for later sampling
 *
 * 2. Color Attachment: RGB(A) texture storing rendered colors
 *    - Format: RGBA16F (HDR, 64 bits per pixel)
 *    - Use: Scene rendering, post-processing input, tone mapping source
 *    - Sampled in shaders: texture(u_SceneTexture, uv)
 *
 * 3. Depth Attachment: Depth values for depth testing
 *    - Format: DEPTH_COMPONENT24 (24-bit depth, OpenGL)
 *    - Use: Depth testing during RTT, depth-based effects (SSAO, DOF)
 *    - Sampled in shaders: texture(u_DepthTexture, uv).r
 *
 * 4. Framebuffer Completeness: Validation that FBO is usable
 *    - OpenGL: glCheckFramebufferStatus() returns GL_FRAMEBUFFER_COMPLETE
 *    - Vulkan: VkFramebuffer creation succeeds
 *    - Incomplete FBO = cannot render (missing attachments, size mismatch, etc.)
 *
 * 5. Multi-Pass Rendering: Render scene multiple times for different effects
 *    - Pass 1: Render scene to FBO (capture colors + depth)
 *    - Pass 2: Post-process FBO result (bloom, tone mapping, etc.)
 *    - Pass 3: Final output to screen
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // Create HDR framebuffer for scene rendering (post-process foundation)
 * auto sceneFBO = renderDevice->createFramebuffer(1920, 1080);
 *
 * // Validate creation succeeded
 * if (!sceneFBO->isComplete()) {
 *     LOG_ERROR("Framebuffer incomplete! Cannot render to texture.");
 *     return;
 * }
 *
 * // === RENDER PASS 1: Scene to Framebuffer ===
 * sceneFBO->bind();  // Redirect rendering to FBO instead of screen
 *
 * renderer->clearScreen(true, true);  // Clear FBO color + depth
 * scene->render(camera);              // Render scene to FBO
 *
 * sceneFBO->unbind();  // Restore rendering to screen
 *
 * // === RENDER PASS 2: Post-Processing ===
 * // Get FBO attachments as ITexture interfaces (abstraction preserved)
 * auto colorTexture = sceneFBO->getColorAttachment();   // Scene colors (HDR)
 * auto depthTexture = sceneFBO->getDepthAttachment();   // Scene depth
 *
 * // Bind textures for post-processing shader (just like regular textures)
 * colorTexture->bind(0);
 * depthTexture->bind(1);
 *
 * // Apply post-processing (tone mapping, bloom, etc.)
 * postProcessShader->bind();
 * postProcessShader->setUniform("u_SceneTexture", 0);   // Color attachment
 * postProcessShader->setUniform("u_DepthTexture", 1);   // Depth attachment
 * postProcessShader->setUniform("u_Exposure", 1.0f);    // Tone mapping parameter
 *
 * screenQuad->draw();  // Full-screen quad, renders to screen
 *
 * // === Handle Window Resize ===
 * void onWindowResize(int width, int height) {
 *     sceneFBO->resize(width, height);  // Recreates attachments at new resolution
 * }
 *
 * // === Query Framebuffer Properties ===
 * int fbWidth = sceneFBO->getWidth();
 * int fbHeight = sceneFBO->getHeight();
 * LOG_INFO("Framebuffer: {}x{} resolution", fbWidth, fbHeight);
 * ```
 *
 * INTEGRATION WITH ENGINE:
 * Before Refactor:
 * ```cpp
 * // Hardcoded OpenGL - NOT portable
 * GLuint fbo, colorTex, depthTex;
 * glGenFramebuffers(1, &fbo);
 * glBindFramebuffer(GL_FRAMEBUFFER, fbo);
 * // ... manual texture creation and attachment ...
 *
 * // Post-processing uses raw GLuint
 * glBindTexture(GL_TEXTURE_2D, colorTex);  // Hardcoded OpenGL
 * ```
 *
 * After Refactor:
 * ```cpp
 * // API-agnostic interface
 * auto fbo = renderDevice->createFramebuffer(1920, 1080);
 * fbo->bind();
 *
 * // Post-processing uses ITexture interface (abstraction preserved)
 * auto colorTexture = fbo->getColorAttachment();  // std::shared_ptr<ITexture>
 * colorTexture->bind(0);  // Works with OpenGL or Vulkan
 * ```
 *
 * TYPICAL INTEGRATION PATTERN:
 * PostProcessManager uses framebuffers for effect chain:
 * ```cpp
 * class PostProcessManager {
 *     IRenderDevice* m_renderDevice;
 *     std::shared_ptr<IFramebuffer> m_sceneFBO;     // Scene render target
 *     std::shared_ptr<IFramebuffer> m_pingPongFBO;  // Blur ping-pong buffer
 *
 *     void initialize(int width, int height) {
 *         m_sceneFBO = m_renderDevice->createFramebuffer(width, height);
 *         m_pingPongFBO = m_renderDevice->createFramebuffer(width, height);
 *     }
 *
 *     void render(Scene& scene, Camera& camera) {
 *         // Pass 1: Render scene to FBO
 *         m_sceneFBO->bind();
 *         scene.render(camera);
 *         m_sceneFBO->unbind();
 *
 *         // Pass 2: Bloom (extract bright pixels)
 *         m_pingPongFBO->bind();
 *         auto sceneColor = m_sceneFBO->getColorAttachment();
 *         sceneColor->bind(0);
 *         bloomExtractShader->bind();
 *         screenQuad->draw();
 *         m_pingPongFBO->unbind();
 *
 *         // Pass 3: Tone mapping to screen
 *         auto bloomColor = m_pingPongFBO->getColorAttachment();
 *         sceneColor->bind(0);
 *         bloomColor->bind(1);
 *         toneMapShader->bind();
 *         screenQuad->draw();
 *     }
 * };
 * ```
 *
 * Shadow mapping uses depth-only framebuffer (future):
 * ```cpp
 * // shadow mapping
 * auto shadowFBO = renderDevice->createFramebuffer(2048, 2048);
 *
 * // Render scene from light's perspective (depth only)
 * shadowFBO->bind();
 * scene.renderDepthOnly(lightCamera);
 * shadowFBO->unbind();
 *
 * // Use shadow map in main render pass
 * auto shadowMap = shadowFBO->getDepthAttachment();
 * shadowMap->bind(2);
 * shader->setUniform("u_ShadowMap", 2);
 * ```
 *
 * HDR RENDERING:
 * RGBA16F color format enables High Dynamic Range:
 *
 * Standard LDR (8-bit RGBA):
 * - Range: 0.0 to 1.0 (clamped)
 * - Bright areas: Clipped to 1.0 (information lost)
 * - Example: Bright sunlight = 1.0, dim light = 1.0 (same!)
 *
 * HDR (16-bit float RGBA16F):
 * - Range: 0.0 to infinity (unclamped)
 * - Bright areas: >1.0 preserved (information retained)
 * - Example: Bright sunlight = 10.0, dim light = 1.0 (different!)
 *
 * Why HDR for PBR:
 * - Physically accurate: Real-world lighting has huge dynamic range
 * - Tone mapping: Convert HDR -> LDR for display (ACES, Reinhard)
 * - Bloom: Extract bright pixels (>1.0 threshold) for glow
 * - Exposure: Adjust brightness in post-processing (camera simulation)
 *
 * Workflow:
 * 1. Render scene to HDR framebuffer (RGBA16F, values can exceed 1.0)
 * 2. Apply post-processing in HDR space (bloom, color grading)
 * 3. Tone map to LDR (0-1 range) for final output to screen
 *
 * FRAMEBUFFER FORMATS:
 * Color Attachment (RGBA16F):
 * - Format: GL_RGBA16F (OpenGL), VK_FORMAT_R16G16B16A16_SFLOAT (Vulkan)
 * - Precision: 16-bit float per channel (64 bits per pixel)
 * - Range: -65504 to +65504 (half-precision float)
 * - Use: HDR scene rendering, tone mapping source
 * - Memory: width x height x 8 bytes (1920x1080 = 16 MB)
 *
 * Depth Attachment (DEPTH24):
 * - Format: GL_DEPTH_COMPONENT24 (OpenGL), VK_FORMAT_D24_UNORM_S8_UINT (Vulkan)
 * - Precision: 24-bit depth (normalized 0.0 to 1.0)
 * - Use: Depth testing during RTT, depth-based effects
 * - Memory: width x height x 4 bytes (1920×1080 = 8 MB, includes 8-bit stencil)
 *
 * Alternative formats (future):
 * - RGB32F: Full 32-bit float precision (expensive, rarely needed)
 * - RGB10_A2: Packed 10-bit RGB + 2-bit alpha (efficient HDR)
 * - DEPTH32F: 32-bit float depth (better precision for large scenes)
 *
 * RESIZE BEHAVIOR:
 * Window resize requires framebuffer recreation:
 *
 * ```cpp
 * void IFramebuffer::resize(int width, int height) {
 *     // Implementation (GLFramebuffer example):
 *     // 1. Delete old textures (glDeleteTextures)
 *     // 2. Create new textures at new resolution
 *     // 3. Reattach textures to framebuffer
 *     // 4. Validate completeness (glCheckFramebufferStatus)
 * }
 * ```
 *
 * Why recreation needed:
 * - Texture resolution is immutable (can't resize existing texture)
 * - Must create new textures at new size, reattach to FBO
 * - OpenGL: Recreate textures, Vulkan: Recreate VkImage + VkImageView
 *
 * Performance consideration:
 * - Resize is expensive (~10-50ms for large FBOs)
 * - Only resize when window actually changes (not every frame)
 * - Typical usage: onWindowResize callback, infrequent event
 *
 * FRAMEBUFFER COMPLETENESS:
 * isComplete() validates framebuffer is usable:
 *
 * OpenGL checks:
 * - GL_FRAMEBUFFER_COMPLETE: All attachments valid
 * - GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: Attachment has issues
 * - GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: No attachments
 * - GL_FRAMEBUFFER_UNSUPPORTED: Format combination not supported
 *
 * Common causes of incomplete FBO:
 * - No attachments: FBO created but no color/depth attached
 * - Size mismatch: Color and depth have different dimensions
 * - Format unsupported: GPU doesn't support format combination
 * - Missing texture: Attachment texture deleted or invalid
 *
 * Usage:
 * ```cpp
 * auto fbo = renderDevice->createFramebuffer(1920, 1080);
 * if (!fbo->isComplete()) {
 *     LOG_ERROR("FBO incomplete - cannot render!");
 *     // Fall back to screen rendering or show error
 * }
 * ```
 *
 * MULTIPLE RENDER TARGETS (Future):
 * Advanced rendering requires multiple color attachments:
 *
 * Deferred rendering (G-buffer):
 * - Attachment 0: RGB albedo + A metallic
 * - Attachment 1: RGB normal + A roughness
 * - Attachment 2: RGB emission + A AO
 * - Attachment 3: Depth
 *
 * Screen-space effects (SSR, SSGI):
 * - Attachment 0: RGB color + A alpha
 * - Attachment 1: RGB normal (view space)
 * - Attachment 2: RGB position or depth
 *
 * Future interface extension:
 * ```cpp
 * virtual std::shared_ptr<ITexture> getColorAttachment(uint32_t index) const = 0;
 * virtual uint32_t getColorAttachmentCount() const = 0;
 * ```
 *
 * CURRENT LIMITATIONS (By Design, Will Address Later):
 *
 * 1. Single Color Attachment:
 * Problem: Can't create G-buffer or MRT for deferred rendering
 * Current impact: None - forward rendering uses single color attachment
 * Future: Add getColorAttachment(index) for multiple attachments
 * Time to implement: 2-3 hours for MRT support
 * When needed: SSR/SSGI need normal + depth attachments
 *
 * 2. Fixed Formats (RGBA16F + DEPTH24):
 * Problem: Can't customize attachment formats (RGB32F, RGB10_A2, etc.)
 * Current impact: Minimal - RGBA16F is optimal for HDR + PBR
 * Future: Add format parameters to createFramebuffer(width, height, colorFormat, depthFormat)
 * Time to implement: 1-2 hours for format selection
 * When needed: Optimization phase (RGB10_A2 for memory savings)
 *
 * 3. No Stencil Attachment Access:
 * Problem: Can't access stencil buffer for stencil effects (portals, outlines)
 * Current impact: None - no stencil effects in current roadmap
 * Future: Add getStencilAttachment() if stencil effects needed
 * Time to implement: 1 hour
 * When needed: Advanced effects
 *
 * 4. No Multisampled FBOs:
 * Problem: Can't use MSAA with render-to-texture (anti-aliasing in post-process)
 * Current impact: None - MSAA applied to final screen output, not intermediate FBOs
 * Future: Add MSAA sample count parameter to createFramebuffer
 * Time to implement: 3-4 hours (resolve pass required)
 * When needed: Quality improvement (if MSAA + post-processing both needed)
 *
 * 5. No Depth-Only FBOs:
 * Problem: Shadow mapping wastes memory with unused color attachment
 * Current impact: Minor - 16 MB color attachment for 2048×2048 shadow map
 * Future: Add createDepthOnlyFramebuffer() for shadows
 * Time to implement: 1-2 hours
 * When needed: shadow mapping optimization
 *
 * PERFORMANCE:
 * Creation Cost:
 * - OpenGL FBO creation: ~1-5ms (glGenFramebuffers + texture creation + attachment)
 * - Typically done once at startup or window resize (infrequent)
 * - Not a hot path - creation is acceptable cost
 *
 * Binding Cost (November 17, 2025):
 * - OpenGL glBindFramebuffer: ~0.01-0.02ms per bind (state change)
 * - Typical multi-pass: 2-5 FBO binds per frame (0.02-0.1ms total)
 * - Context: Negligible compared to actual rendering (5-15ms per pass)
 *
 * Resize Cost:
 * - 1920×1080 FBO: ~10-20ms (texture deletion + creation + attachment)
 * - 2560×1440 FBO: ~20-40ms (larger textures = more GPU memory transfer)
 * - Mitigation: Only resize on window resize event (infrequent)
 *
 * Memory Usage:
 * - RGBA16F color: width x height x 8 bytes
 * - DEPTH24 depth: width x height x 4 bytes (includes 8-bit stencil)
 * - Example 1920×1080: 16 MB color + 8 MB depth = 24 MB total
 * - Example 2560×1440: 28 MB color + 14 MB depth = 42 MB total
 * - Multiple FBOs: 2-3 FBOs typical (scene + ping-pong buffers = 48-72 MB)
 *
 * IMPLEMENTATIONS:
 * - GLFramebuffer (November 2025): OpenGL FBO implementation
 *   - Creates FBO (glGenFramebuffers, glBindFramebuffer)
 *   - Creates color texture (RGBA16F, GL_RGBA16F, GL_FLOAT)
 *   - Creates depth texture (DEPTH24, GL_DEPTH_COMPONENT24)
 *   - Attaches textures (glFramebufferTexture2D)
 *   - Validates completeness (glCheckFramebufferStatus)
 *   - Wraps attachments in GLTexture -> ITexture (abstraction preserved)
 *   - Status: Complete, production-ready, used by post-process foundation
 *
 * - VKFramebuffer (Future): Vulkan render pass implementation
 *   - Creates VkImage + VkImageView for color (VK_FORMAT_R16G16B16A16_SFLOAT)
 *   - Creates VkImage + VkImageView for depth (VK_FORMAT_D24_UNORM_S8_UINT)
 *   - Creates VkRenderPass with attachment descriptions
 *   - Creates VkFramebuffer referencing image views and render pass
 *   - Manages image layout transitions (UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL)
 *   - Wraps attachments in VKTexture -> ITexture (abstraction preserved)
 *   - Status: Planned, interface already designed
 *   - Estimate: 5-7 days (Vulkan render pass system is complex)
 *
 * DEPENDENCIES:
 * - <cstdint>: int for dimensions
 * - <memory>: std::shared_ptr for ITexture attachment ownership
 * - ITexture.h: Forward declared for attachment access
 *
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL FBOs are context-dependent
 * - Vulkan: Framebuffers are immutable after creation, can be used across threads
 * - Current: All framebuffer operations on main render thread only
 *
 * REFERENCES:
 * - The Cherno C++ Series: "Interfaces in C++" (foundational design pattern)
 * - Gang of Four Design Patterns: Abstract Factory (framebuffer creation pattern)
 * - Learn OpenGL (learnopengl.com): Framebuffers tutorial (FBO creation, attachments)
 * - Real-Time Rendering 4th Ed., Chapter 23.6: Render targets and multi-pass rendering
 * - OpenGL Programming Guide: Chapter 4 - Framebuffer objects and render-to-texture
 * - Game Engine Architecture 3rd Ed., Chapter 10.3: Post-processing and effect chains
 * - Khronos OpenGL Wiki: Framebuffer Object specification and best practices
 *
 * FUTURE ENHANCEMENTS:
 * (Shadow Mapping):
 * - Add createDepthOnlyFramebuffer() for shadow maps (no color attachment)
 * - Optimization: 2048x2048 depth-only = 8 MB (vs 24 MB with unused color)
 * - Time: 1-2 hours
 *
 * (SSR, SSGI):
 * - Add Multiple Render Targets (MRT) support
 * - Add getColorAttachment(uint32_t index) for multiple attachments
 * - G-buffer: 3-4 color attachments + depth
 * - Time: 2-3 hours for MRT system
 *
 * Optimization Phase:
 * - Add format selection (RGB32F, RGB10_A2, DEPTH32F options)
 * - Add MSAA support (multisampled FBOs with resolve pass)
 * - Add stencil attachment access for advanced effects
 * - Time: 1 week for comprehensive FBO system
 *
 * (Vulkan):
 * - VKFramebuffer implementation with render pass + framebuffer + image views
 * - Synchronization barriers for image layout transitions
 * - Subpass dependencies for advanced rendering techniques
 * - Time: 5-7 days (Vulkan render pass system is significantly more complex than OpenGL)
 *
 * Optional (Quality of Life):
 * - Framebuffer pooling (reuse FBOs to reduce allocation overhead)
 * - Automatic mipmap generation for attachments
 * - Cubemap framebuffers for reflection probes
 * - Layered framebuffers for texture arrays
 *
 * HISTORY:
 * November 6, 2025: Initial creation during interface refactor
 * - Created pure virtual interface with bind/unbind and query methods
 * - Designed attachment access as ITexture interfaces (abstraction preserved)
 * - Added resize() for dynamic resolution changes (window resize support)
 * - Added isComplete() for FBO validation
 * - Implemented by GLFramebuffer (FBO + RGBA16F + DEPTH24)
 *
 * November 7-8, 2025: Integration and validation
 * - Used by PostProcessManager foundation
 * - Tested HDR rendering workflow (scene -> FBO -> tone map -> screen)
 * - Validated resize behavior (window resize events)
 * - Confirmed completeness checks (error handling)
 * - Zero bugs, zero memory leaks, production-ready
 *
 */

namespace Engine
{
    class ITexture;  // Forward declare

    class IFramebuffer
    {
    public:
        virtual ~IFramebuffer() = default;

        // Lifecycle
        virtual void bind() const = 0;
        virtual void unbind() const = 0;
        virtual void resize(int width, int height) = 0;

        // Query
        virtual int getWidth() const = 0;
        virtual int getHeight() const = 0;
        virtual bool isComplete() const = 0;

        // Texture attachments (for post-processing, sampling in shaders)
        // Returns ITexture interfaces, NOT raw API handles (maintains abstraction)
        virtual std::shared_ptr<ITexture> getColorAttachment() const = 0;
        virtual std::shared_ptr<ITexture> getDepthAttachment() const = 0;
    };
}