#pragma once
#include "renderer/interface/IRenderDevice.h"

/*
 * GLRenderDevice.h
 *
 * PURPOSE:
 * OpenGL-specific factory implementing IRenderDevice interface. Creates all OpenGL rendering
 * resources (shaders, textures, meshes, framebuffers) and returns them as interface pointers.
 * Enables renderer abstraction - 90% of engine code is API-agnostic, switching to Vulkan
 * requires changing only this factory instantiation.
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Engine hardcoded to OpenGL (Shader, Texture, Mesh everywhere). Adding Vulkan
 * would require 6+ months rewriting entire codebase. Need abstraction layer separating
 * API-specific implementation from engine logic.
 *
 * Solution: Abstract Factory pattern with interface layer.
 * - Interfaces: IShader, ITexture, IMesh, IFramebuffer, IDebugRenderer (API-agnostic)
 * - Implementations: GLShader, GLTexture, GLMesh, etc. (OpenGL-specific)
 * - Factory: GLRenderDevice creates GL* objects, returns as I* pointers
 * - Result: 90% of code uses interfaces, 10% is GL/Vulkan implementations
 *
 * Key Insight: Abstract Factory pattern perfect for renderer abstraction. Engine code
 * uses interfaces (IShader::bind), implementations hidden. Changing renderer = change
 * factory instantiation, everything else unchanged.
 *
 * DESIGN PHILOSOPHY:
 * - Abstract Factory: Hide implementation details behind interface
 * - Stateless: No internal state, pure factory methods
 * - Validation: Comprehensive parameter checking (null checks, ranges)
 * - Automatic lifetime: Returns shared_ptr for RAII resource management
 * - Dependency injection: Pass factory to managers/systems
 *
 * KEY CONCEPTS:
 * 1. Abstract Factory Pattern:
 *    - IRenderDevice: Abstract factory interface
 *    - GLRenderDevice: Concrete factory (OpenGL)
 *    - VKRenderDevice: Future concrete factory (Vulkan)
 *    - Created objects: Returned as interface pointers
 *
 * 2. Resource Creation:
 *    - Shaders: From source strings or file paths
 *    - Textures: From image files (PNG, JPG)
 *    - Meshes: From vertex/index data
 *    - Framebuffers: HDR color + depth attachments
 *    - Debug renderers: Line-based visualization
 *
 * 3. Validation:
 *    - Null pointer checks (source code, file paths, data)
 *    - Range validation (counts > 0, dimensions <= 16384)
 *    - ENGINE_ASSERT in debug (crash with message)
 *    - Graceful fallbacks in release (magenta checkerboard textures)
 *
 * 4. API Switching:
 *    - Change one line: GLRenderDevice -> VKRenderDevice
 *    - Rest of engine unchanged (uses interfaces)
 *    - Compile-time or runtime switching possible
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === APPLICATION STARTUP (Choose Renderer) ===
 * #ifdef USE_OPENGL
 *     auto renderDevice = std::make_unique<GLRenderDevice>();
 * #else
 *     auto renderDevice = std::make_unique<VKRenderDevice>();
 * #endif
 *
 * // === DEPENDENCY INJECTION (Pass to Managers) ===
 * AssetManager::get().initialize(renderDevice.get());
 * ShaderManager::get().initialize(renderDevice.get());
 * MeshFactory::initialize(renderDevice.get());
 *
 * // === RESOURCE CREATION (Through Interface) ===
 * // Create shader
 * auto shader = renderDevice->createShaderFromFiles(
 *     "shaders/basic.vert",
 *     "shaders/basic.frag"
 * );
 *
 * // Create texture (loads from file, creates fallback on failure)
 * auto texture = renderDevice->createTexture("textures/wood.png");
 *
 * // Create mesh (indexed geometry)
 * auto mesh = renderDevice->createMesh(
 *     vertices,           // float* vertex data
 *     vertexDataSize,     // size in bytes
 *     indices,            // uint32_t* index data
 *     indexCount,         // number of indices
 *     VertexFormat::P3N3T2  // Position + Normal + TexCoord
 * );
 *
 * // Create framebuffer (HDR color + depth)
 * auto framebuffer = renderDevice->createFramebuffer(1920, 1080);
 *
 * // Create debug renderer (line visualization)
 * auto debugRenderer = renderDevice->createDebugRenderer();
 *
 * // === USE POLYMORPHICALLY (Works with GL or Vulkan) ===
 * shader->bind();                          // IShader::bind()
 * texture->bind(0);                        // ITexture::bind()
 * mesh->draw();                            // IMesh::draw()
 * framebuffer->bind();                     // IFramebuffer::bind()
 * debugRenderer->drawLine(start, end);     // IDebugRenderer::drawLine()
 *
 * // === QUERY GPU INFO ===
 * LOG_INFO("Device: {}", renderDevice->getDeviceName());      // "OpenGL 4.6"
 * LOG_INFO("GPU: {}", renderDevice->getRendererName());       // "NVIDIA GeForce RTX 3090 Ti"
 * ```
 *
 * FACTORY METHODS:
 *
 * Shader creation:
 * ```cpp
 * // From source strings (manual compilation)
 * std::shared_ptr<IShader> createShader(
 *     const char* vertexSource,
 *     const char* fragmentSource
 * );
 *
 * // From files (convenience, loads + compiles)
 * std::shared_ptr<IShader> createShaderFromFiles(
 *     const char* vertexPath,
 *     const char* fragmentPath
 * );
 * ```
 * - Validation: Non-null source/paths, valid GLSL syntax
 * - Returns: GLShader wrapped in IShader interface pointer
 * - Failure: Logs error, returns nullptr (caller handles)
 *
 * Texture creation:
 * ```cpp
 * std::shared_ptr<ITexture> createTexture(const char* filepath);
 * ```
 * - Validation: Non-null path, file exists, valid image format
 * - Returns: GLTexture with mipmaps, anisotropic filtering
 * - Failure: Creates magenta checkerboard fallback (engine keeps running)
 * - Caching: AssetManager caches by filepath (prevents duplicates)
 *
 * Mesh creation (indexed):
 * ```cpp
 * std::shared_ptr<IMesh> createMesh(
 *     const float* vertices,
 *     size_t vertexDataSize,
 *     const uint32_t* indices,
 *     size_t indexCount,
 *     VertexFormat format
 * );
 * ```
 * - Validation: Non-null data, counts > 0, format valid
 * - Returns: GLMesh with VAO/VBO/EBO, bounding volumes
 * - Format: Position, Normal, TexCoord layout (VertexFormat enum)
 * - Bounding volumes: Calculated automatically from vertex data
 *
 * Framebuffer creation:
 * ```cpp
 * std::shared_ptr<IFramebuffer> createFramebuffer(int width, int height);
 * ```
 * - Validation: width/height > 0 and <= 16384
 * - Returns: GLFramebuffer with HDR color (GL_RGBA16F) + depth (GL_DEPTH_COMPONENT24)
 * - Use cases: Post-processing, shadow mapping, reflections
 * - Render device injection: Framebuffer needs factory to create texture wrappers
 *
 * Debug renderer creation:
 * ```cpp
 * std::shared_ptr<IDebugRenderer> createDebugRenderer();
 * ```
 * - Returns: GLDebugRenderer for line-based visualization
 * - Use cases: Bounding volumes, frustum planes, coordinate axes
 * - Lightweight: Single VAO/VBO, immediate-mode style API
 *
 * VALIDATION STRATEGY:
 *
 * Debug builds (ENGINE_ASSERT):
 * ```cpp
 * std::shared_ptr<IShader> GLRenderDevice::createShader(
 *     const char* vertexSource,
 *     const char* fragmentSource)
 * {
 *     ENGINE_ASSERT(vertexSource != nullptr, "Vertex source is null");
 *     ENGINE_ASSERT(fragmentSource != nullptr, "Fragment source is null");
 *
 *     return std::make_shared<GLShader>(vertexSource, fragmentSource);
 * }
 * ```
 * - Crashes with detailed message (developer error)
 * - Helps catch bugs early in development
 *
 * Release builds (graceful handling):
 * - Null checks return nullptr or fallback resource
 * - Errors logged but engine continues
 * - Example: Missing texture -> magenta checkerboard
 *
 * API SWITCHING - How It Works:
 *
 * Current (OpenGL):
 * ```cpp
 * // Application.cpp
 * auto renderDevice = std::make_unique<GLRenderDevice>();
 * ```
 *
 * Future (Vulkan):
 * ```cpp
 * // Application.cpp - ONE LINE CHANGE
 * auto renderDevice = std::make_unique<VKRenderDevice>();
 * ```
 *
 * Everything else unchanged:
 * - Scene.h: Uses IShader& (not GLShader or VKShader)
 * - Material.h: Uses ITexture (not GLTexture or VKTexture)
 * - SceneObject.h: Uses IMesh (not GLMesh or VKMesh)
 * - AssetManager: Uses IRenderDevice* (not GLRenderDevice*)
 * - Result: 90% of code is API-agnostic
 *
 * What changes for Vulkan:
 * 1. Implement VKRenderDevice factory (Week 20+)
 * 2. Implement VKShader, VKTexture, VKMesh, etc. (Week 20+)
 * 3. Change factory instantiation (one line)
 * 4. Recompile, done!
 *
 * DEVICE INFORMATION:
 *
 * ```cpp
 * const char* getDeviceName() const;      // "OpenGL 4.6"
 * const char* getRendererName() const;    // GPU name from driver
 * ```
 *
 * Implementation:
 * - getDeviceName(): Returns "OpenGL X.Y" (hardcoded)
 * - getRendererName(): Queries GL_RENDERER (actual GPU from driver)
 * - Example: "NVIDIA GeForce RTX 3090 Ti/PCIe/SSE2"
 * - Used for: Logging, bug reports, GPU-specific workarounds
 *
 * CURRENT STATE (November 6, 2025):
 * - Creates all core rendering resources
 * - Full validation with detailed error messages
 * - GPU query support (getRendererName)
 * - Framebuffer receives render device (for texture wrappers)
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Stateless Factory:
 * - No capability queries (maxTextureSize, supportsCompute)
 * - No resource tracking (allocated VRAM, active objects)
 * - Future: Add query methods (Week 8+)
 *
 * 2. No Resource Pooling:
 * - Each createFramebuffer allocates new FBO
 * - No reuse of staging buffers
 * - Future: Add pool system (Week 10+)
 *
 * 3. No Statistics:
 * - Can't query draw calls, state changes, VRAM usage
 * - Future: Add statistics API (Week 8+)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * November 6, 2025: Initial implementation
 * - Abstract Factory pattern
 * - Creates all GL resources
 * - Returns interface pointers
 *
 * (Enhancements):
 * - Capability queries (maxTextureSize, etc.)
 * - Resource statistics (VRAM tracking)
 * - Hot-reload integration (ShaderManager handles)
 *
 * (Vulkan Renderer):
 * - Implement VKRenderDevice factory
 * - Implement VK* resource classes
 * - Runtime or compile-time API switching
 * - Time: 2-3 weeks for full Vulkan implementation
 *
 * DEPENDENCIES:
 * - renderer/interface/IRenderDevice.h: Abstract factory interface
 * - renderer/opengl/GLShader.h: OpenGL shader implementation
 * - renderer/opengl/GLTexture.h: OpenGL texture implementation
 * - renderer/opengl/GLMesh.h: OpenGL mesh implementation
 * - renderer/opengl/GLFramebuffer.h: OpenGL framebuffer implementation
 * - renderer/opengl/GLDebugRenderer.h: OpenGL debug renderer implementation
 * - renderer/opengl/GLRenderer.h: OpenGL renderer implementation
 *
 * THREAD SAFETY:
 * - Stateless: Factory has no mutable state
 * - Creation: NOT thread-safe (OpenGL context requirement)
 * - All creation on main render thread only
 * - Created resources: Thread safety depends on implementation
 *
 * REFERENCES:
 * - Design Patterns (Gang of Four): Abstract Factory pattern
 * - IRenderDevice.h: Interface documentation
 * - refactor: Interface abstraction rationale
 *
 * HISTORY:
 * November 6, 2025: Initial implementation
 * - Abstract Factory pattern for renderer abstraction
 * - Creates all core OpenGL resources
 * - Returns interface pointers (API-agnostic)
 * - Full validation (ENGINE_ASSERT + graceful fallbacks)
 * - GPU query support (getRendererName)
 * - Result: 90% of engine code now API-agnostic
 *
 */

namespace Engine
{
    class GLRenderDevice : public IRenderDevice
    {
    public:
        GLRenderDevice() = default;
        ~GLRenderDevice() override = default;

        // IRenderDevice interface implementation

        // Shader creation (from source strings)
        std::shared_ptr<IShader> createShader(
            const char* vertexSource,
            const char* fragmentSource) override;

        // Shader creation (from files - convenience method)
        std::shared_ptr<IShader> createShaderFromFiles(
            const char* vertexPath,
            const char* fragmentPath) override;

        // Texture creation (loads from file, creates fallback on failure)
        std::shared_ptr<ITexture> createTexture(const char* filepath) override;

        // Mesh creation - non-indexed (simple geometry)
        std::shared_ptr<IMesh> createMesh(
            const float* vertices,
            size_t dataSize,
            uint32_t vertexCount) override;

        // Mesh creation - indexed (standard workflow, Week 3)
        std::shared_ptr<IMesh> createMesh(
            const float* vertices,
            size_t vertexDataSize,
            const uint32_t* indices,
            size_t indexCount,
            VertexFormat format) override;

        // Framebuffer creation (HDR color + depth, for post-processing)
        std::shared_ptr<IFramebuffer> createFramebuffer(
            int width,
            int height) override;

        // Debug renderer creation (line-based visualization)
        std::shared_ptr<IDebugRenderer> createDebugRenderer() override;

        // Renderer creation
        std::unique_ptr<IRenderer> createRenderer() override;

        // Device information
        const char* getDeviceName() const override;      // "OpenGL 4.6"
        const char* getRendererName() const override;    // GPU name from driver
    };
}