#include "renderer/PostProcessManager.h"
#include "renderer/ShaderManager.h"
#include "renderer/interface/ITexture.h"
#include "core/Logger.h"

namespace Engine
{
    PostProcessManager::PostProcessManager(Window* window, IRenderDevice* renderDevice, IRenderer* renderer)
        : m_window(window)
        , m_renderDevice(renderDevice)
        , m_renderer(renderer)
        , m_screenQuad()
        , m_lastWidth(window->getWidth())
        , m_lastHeight(window->getHeight())
    {
        // Create framebuffer via render device (abstraction layer)
        m_framebuffer = renderDevice->createFramebuffer(
            window->getWidth(),
            window->getHeight()
        );

        // Load post-process shader (ShaderManager uses render device internally)
        m_postProcessShader = ShaderManager::get().loadShader(
            "postprocess",
            "assets/shaders/postprocess.vert",
            "assets/shaders/postprocess.frag"
        );

        if (!m_postProcessShader)
        {
            LOG_ERROR("Failed to load post-process shader!");
        }

        LOG_INFO("PostProcessManager initialized ({}x{})",
            window->getWidth(), window->getHeight());
    }

    void PostProcessManager::beginScene()
    {
        // Check if window size changed (handle dynamic resize)
        int currentWidth = m_window->getWidth();
        int currentHeight = m_window->getHeight();

        if (currentWidth != m_lastWidth || currentHeight != m_lastHeight)
        {
            // Window resized - recreate framebuffer with new dimensions
            LOG_INFO("PostProcess: Window resized from {}x{} to {}x{}, recreating framebuffer",
                m_lastWidth, m_lastHeight, currentWidth, currentHeight);

            m_framebuffer = m_renderDevice->createFramebuffer(currentWidth, currentHeight);
            m_lastWidth = currentWidth;
            m_lastHeight = currentHeight;
        }

        // Bind framebuffer (redirect rendering to texture)
        m_framebuffer->bind();

        m_renderer->setViewport(0, 0, currentWidth, currentHeight);
    }

    void PostProcessManager::endScene()
    {
        m_framebuffer->unbind();

        int width = m_window->getWidth();
        int height = m_window->getHeight();

        m_renderer->setViewport(0, 0, width, height);
        m_renderer->clearScreen(true, true, false);
        m_renderer->beginPostProcessPass();  // Depth OFF

        if (m_postProcessShader)
        {
            m_postProcessShader->bind();
            m_postProcessShader->setUniform("u_ScreenTexture", 0);
            m_postProcessShader->setUniform("u_GrayscaleEnabled", m_grayscaleEnabled);

            auto colorTexture = m_framebuffer->getColorAttachment();
            colorTexture->bind(0);

            m_screenQuad.render();

            colorTexture->unbind();
            m_postProcessShader->unbind();
        }

        m_renderer->endPass();  // Restore depth
    }

    int PostProcessManager::getFramebufferWidth() const
    {
        return m_framebuffer ? m_framebuffer->getWidth() : 0;
    }

    int PostProcessManager::getFramebufferHeight() const
    {
        return m_framebuffer ? m_framebuffer->getHeight() : 0;
    }
}