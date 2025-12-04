#include "renderer/opengl/GLFramebuffer.h"
#include "renderer/opengl/GLTexture.h"
#include "core/Logger.h"

namespace Engine
{
    GLFramebuffer::GLFramebuffer(int width, int height, IRenderDevice* renderDevice)
        : m_width(width), m_height(height), m_renderDevice(renderDevice)
    {
        create();
        createTextureWrappers();
        LOG_INFO("GLFramebuffer created ({}x{})", width, height);
    }

    GLFramebuffer::~GLFramebuffer()
    {
        destroy();
    }

    GLFramebuffer::GLFramebuffer(GLFramebuffer&& other) noexcept
        : m_fbo(other.m_fbo)
        , m_colorTexture(other.m_colorTexture)
        , m_depthRBO(other.m_depthRBO)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_isComplete(other.m_isComplete)
        , m_colorAttachment(std::move(other.m_colorAttachment))
        , m_depthAttachment(std::move(other.m_depthAttachment))
        , m_renderDevice(other.m_renderDevice)
    {
        // Leave other empty
        other.m_fbo = 0;
        other.m_colorTexture = 0;
        other.m_depthRBO = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_isComplete = false;
        other.m_renderDevice = nullptr;
    }

    GLFramebuffer& GLFramebuffer::operator=(GLFramebuffer&& other) noexcept
    {
        if (this != &other)
        {
            // Destroy current resources
            destroy();

            // Steal from other
            m_fbo = other.m_fbo;
            m_colorTexture = other.m_colorTexture;
            m_depthRBO = other.m_depthRBO;
            m_width = other.m_width;
            m_height = other.m_height;
            m_isComplete = other.m_isComplete;
            m_colorAttachment = std::move(other.m_colorAttachment);
            m_depthAttachment = std::move(other.m_depthAttachment);
            m_renderDevice = other.m_renderDevice;

            // Leave other empty
            other.m_fbo = 0;
            other.m_colorTexture = 0;
            other.m_depthRBO = 0;
            other.m_width = 0;
            other.m_height = 0;
            other.m_isComplete = false;
            other.m_renderDevice = nullptr;
        }
        return *this;
    }

    void GLFramebuffer::create()
    {
        // Generate framebuffer object
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        // Create color texture attachment (RGBA16F for HDR support)
        glGenTextures(1, &m_colorTexture);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
            GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Attach color texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_colorTexture, 0);

        LOG_INFO("Color attachment created: Texture ID {}", m_colorTexture);

        // Create depth + stencil renderbuffer
        glGenRenderbuffers(1, &m_depthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        // Attach depth + stencil to framebuffer
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, m_depthRBO);

        LOG_INFO("Depth attachment created: Renderbuffer ID {}", m_depthRBO);

        // Check if framebuffer is complete
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        m_isComplete = (status == GL_FRAMEBUFFER_COMPLETE);

        if (m_isComplete)
        {
            LOG_INFO("Framebuffer complete and valid (ID: {})", m_fbo);
        }
        else
        {
            LOG_ERROR("Framebuffer incomplete! Status: 0x{:X}", status);

            // Log specific error
            switch (status)
            {
            case GL_FRAMEBUFFER_UNDEFINED:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_UNDEFINED");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_UNSUPPORTED");
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                LOG_ERROR("  Reason: GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
                break;
            default:
                LOG_ERROR("  Reason: Unknown (0x{:X})", status);
                break;
            }
        }

        // Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFramebuffer::createTextureWrappers()
    {
        // Wrap color texture (RGBA16F = 4 channels)
        m_colorAttachment = std::make_shared<GLTextureView>(
            m_colorTexture,
            m_width,
            m_height,
            4  // RGBA
        );

        // Wrap depth renderbuffer (1 channel for depth)
        // NOTE: This is a renderbuffer, not a texture, so it can't be sampled in shaders
        // When SSAO/SSR/DOF are implemented (Week 11+), convert to depth texture instead
        m_depthAttachment = std::make_shared<GLTextureView>(
            m_depthRBO,
            m_width,
            m_height,
            1  // Depth only
        );
    }

    void GLFramebuffer::destroy()
    {
        // Clear texture wrappers first
        m_colorAttachment.reset();
        m_depthAttachment.reset();

        if (m_colorTexture != 0)
        {
            glDeleteTextures(1, &m_colorTexture);
            m_colorTexture = 0;
        }

        if (m_depthRBO != 0)
        {
            glDeleteRenderbuffers(1, &m_depthRBO);
            m_depthRBO = 0;
        }

        if (m_fbo != 0)
        {
            glDeleteFramebuffers(1, &m_fbo);
            LOG_INFO("Framebuffer destroyed (ID: {})", m_fbo);
            m_fbo = 0;
        }

        m_isComplete = false;
    }

    void GLFramebuffer::bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void GLFramebuffer::unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Note: Caller should set viewport back to window size
    }

    void GLFramebuffer::resize(int width, int height)
    {
        // Skip if already correct size
        if (width == m_width && height == m_height)
            return;

        LOG_INFO("Resizing framebuffer from {}x{} to {}x{}",
            m_width, m_height, width, height);

        // Update dimensions
        m_width = width;
        m_height = height;

        // Recreate with new size
        destroy();
        create();
        createTextureWrappers();  // Recreate wrappers with new texture IDs
    }
}