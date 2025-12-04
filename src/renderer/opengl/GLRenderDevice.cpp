#include "renderer/opengl/GLRenderDevice.h"
#include "renderer/opengl/GLShader.h"
#include "renderer/opengl/GLTexture.h"
#include "renderer/opengl/GLMesh.h"
#include "renderer/opengl/GLFramebuffer.h"
#include "renderer/opengl/GLDebugRenderer.h"
#include "renderer/opengl/GLRenderer.h"
#include "core/Logger.h"
#include "core/Error.h"
#include "core/FileUtils.h"

namespace Engine
{
    std::shared_ptr<IShader> GLRenderDevice::createShader(
        const char* vertexSource,
        const char* fragmentSource)
    {
        ENGINE_ASSERT(vertexSource != nullptr, "Vertex shader source cannot be null");
        ENGINE_ASSERT(fragmentSource != nullptr, "Fragment shader source cannot be null");

        return std::make_shared<GLShader>(vertexSource, fragmentSource);
    }

    std::shared_ptr<IShader> GLRenderDevice::createShaderFromFiles(
        const char* vertexPath,
        const char* fragmentPath)
    {
        ENGINE_ASSERT(vertexPath != nullptr, "Vertex shader path cannot be null");
        ENGINE_ASSERT(fragmentPath != nullptr, "Fragment shader path cannot be null");

        LOG_INFO("Loading shaders from files:");
        LOG_INFO("  Vertex: {}", vertexPath);
        LOG_INFO("  Fragment: {}", fragmentPath);

        // Use FileUtils (which already has proper error handling with asserts)
        std::string vertexSource = FileUtils::readFile(vertexPath);
        std::string fragmentSource = FileUtils::readFile(fragmentPath);

        // FileUtils::readFile() asserts on failure in debug, returns empty in release
        // Double-check for safety
        ENGINE_ASSERT(!vertexSource.empty(), "Vertex shader source is empty after loading");
        ENGINE_ASSERT(!fragmentSource.empty(), "Fragment shader source is empty after loading");

        // Create shader from SOURCE CODE (not file paths)
        return std::make_shared<GLShader>(vertexSource.c_str(), fragmentSource.c_str());
    }

    std::shared_ptr<ITexture> GLRenderDevice::createTexture(const char* filepath)
    {
        ENGINE_ASSERT(filepath != nullptr, "Texture filepath cannot be null");

        // GLTexture constructor handles file loading and fallback texture creation
        return std::make_shared<GLTexture>(filepath);
    }

    std::shared_ptr<IMesh> GLRenderDevice::createMesh(
        const float* vertices,
        size_t dataSize,
        uint32_t vertexCount)
    {
        ENGINE_ASSERT(vertices != nullptr, "Vertex data cannot be null");
        ENGINE_ASSERT(dataSize > 0, "Vertex data size must be > 0");
        ENGINE_ASSERT(vertexCount > 0, "Vertex count must be > 0");

        return std::make_shared<GLMesh>(vertices, dataSize, vertexCount);
    }

    std::shared_ptr<IMesh> GLRenderDevice::createMesh(
        const float* vertices,
        size_t vertexDataSize,
        const uint32_t* indices,
        size_t indexCount,
        VertexFormat format)
    {
        ENGINE_ASSERT(vertices != nullptr, "Vertex data cannot be null");
        ENGINE_ASSERT(indices != nullptr, "Index data cannot be null");
        ENGINE_ASSERT(vertexDataSize > 0, "Vertex data size must be > 0");
        ENGINE_ASSERT(indexCount > 0, "Index count must be > 0");

        return std::make_shared<GLMesh>(vertices, vertexDataSize, indices, indexCount, format);
    }

    std::shared_ptr<IFramebuffer> GLRenderDevice::createFramebuffer(
        int width,
        int height)
    {
        ENGINE_ASSERT(width > 0, "Framebuffer width must be > 0, got {}", width);
        ENGINE_ASSERT(height > 0, "Framebuffer height must be > 0, got {}", height);
        ENGINE_ASSERT(width <= 16384, "Framebuffer width too large: {} (max 16384)", width);
        ENGINE_ASSERT(height <= 16384, "Framebuffer height too large: {} (max 16384)", height);

        // FIXED: Pass 'this' so GLFramebuffer can create texture wrappers
        return std::make_shared<GLFramebuffer>(width, height, this);
    }

    std::shared_ptr<IDebugRenderer> GLRenderDevice::createDebugRenderer()
    {
        return std::make_shared<GLDebugRenderer>();
    }

    std::unique_ptr<IRenderer> GLRenderDevice::createRenderer()
    {
        return std::make_unique<GLRenderer>(this);
    }

    const char* GLRenderDevice::getDeviceName() const
    {
        return "OpenGL 4.6";
    }

    const char* GLRenderDevice::getRendererName() const
    {
        // Query OpenGL for GPU name
        const GLubyte* renderer = glGetString(GL_RENDERER);
        return reinterpret_cast<const char*>(renderer);
    }
}