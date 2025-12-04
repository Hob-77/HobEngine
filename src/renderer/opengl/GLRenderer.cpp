#include "renderer/opengl/GLRenderer.h"
#include "core/Logger.h"
#include "core/Error.h"
#include <glad/glad.h>

namespace Engine
{
    GLRenderer::GLRenderer(IRenderDevice* device)
        : m_renderDevice(device)
    {
        ENGINE_ASSERT(device != nullptr, "RenderDevice cannot be null");

        // Initialize with OpenGL defaults (query current state)
        // This ensures we start in sync with actual GPU state

        m_currentState.depthTest = true;
        m_currentState.depthWrite = true;
        m_currentState.depthFunc = DepthFunc::Less;
        m_currentState.blending = false;
        m_currentState.faceCulling = false;
        m_currentState.frontFace = FrontFace::CCW;
        m_currentState.polygonMode = PolygonMode::Fill;
        m_currentState.lineWidth = 1.0f;
        m_currentState.msaa = false;
        m_currentState.alphaToCoverage = false;

        // Copy to pending (start in sync)
        m_pendingState = m_currentState;

        m_initialized = true;
        LOG_INFO("GLRenderer initialized");
    }

    // === DEPTH ===

    void GLRenderer::setDepthTest(bool enabled)
    {
        m_pendingState.depthTest = enabled;
        flushState();
    }

    void GLRenderer::setDepthWrite(bool enabled)
    {
        m_pendingState.depthWrite = enabled;
        flushState();
    }

    void GLRenderer::setDepthFunc(DepthFunc func)
    {
        m_pendingState.depthFunc = func;
        flushState();
    }

    // === BLENDING ===

    void GLRenderer::setBlending(bool enabled, BlendMode mode)
    {
        m_pendingState.blending = enabled;
        m_pendingState.blendMode = mode;
        flushState();
    }

    // === CULLING ===

    void GLRenderer::setFaceCulling(bool enabled, CullMode mode)
    {
        m_pendingState.faceCulling = enabled;
        m_pendingState.cullMode = mode;
        flushState();
    }

    void GLRenderer::setFrontFace(FrontFace face)
    {
        m_pendingState.frontFace = face;
        flushState();
    }

    // === POLYGON ===

    void GLRenderer::setPolygonMode(PolygonMode mode)
    {
        m_pendingState.polygonMode = mode;
        flushState();
    }

    void GLRenderer::setLineWidth(float width)
    {
        m_pendingState.lineWidth = width;
        flushState();
    }

    // === MSAA ===

    void GLRenderer::setMSAA(bool enabled)
    {
        m_pendingState.msaa = enabled;
        flushState();
    }

    void GLRenderer::setAlphaToCoverage(bool enabled)
    {
        m_pendingState.alphaToCoverage = enabled;
        flushState();
    }

    // === VIEWPORT ===

    void GLRenderer::setViewport(int x, int y, int width, int height)
    {
        m_pendingState.viewportX = x;
        m_pendingState.viewportY = y;
        m_pendingState.viewportWidth = width;
        m_pendingState.viewportHeight = height;
        flushState();
    }

    // === CLEAR ===

    void GLRenderer::clearColor(float r, float g, float b, float a)
    {
        m_pendingState.clearR = r;
        m_pendingState.clearG = g;
        m_pendingState.clearB = b;
        m_pendingState.clearA = a;

        // Apply immediately (doesn't benefit from caching)
        glClearColor(r, g, b, a);
        m_currentState.clearR = r;
        m_currentState.clearG = g;
        m_currentState.clearB = b;
        m_currentState.clearA = a;
    }

    void GLRenderer::clearScreen(bool color, bool depth, bool stencil)
    {
        GLbitfield mask = 0;
        if (color) mask |= GL_COLOR_BUFFER_BIT;
        if (depth) mask |= GL_DEPTH_BUFFER_BIT;
        if (stencil) mask |= GL_STENCIL_BUFFER_BIT;

        if (mask != 0)
        {
            glClear(mask);
        }
    }

    // === RENDER PASSES ===

    void GLRenderer::beginOpaquePass()
    {
        // Save current state
        m_stateStack.push_back(m_currentState);

        // Configure for opaque rendering
        m_pendingState.depthTest = true;
        m_pendingState.depthWrite = true;
        m_pendingState.depthFunc = DepthFunc::Less;
        m_pendingState.blending = false;
        m_pendingState.faceCulling = true;
        m_pendingState.cullMode = CullMode::Back;

        flushState();
    }

    void GLRenderer::beginTransparentPass()
    {
        // Save current state
        m_stateStack.push_back(m_currentState);

        // Configure for transparent rendering
        m_pendingState.depthTest = true;        // Read depth
        m_pendingState.depthWrite = false;      // Don't write depth
        m_pendingState.blending = true;         // Enable blending
        m_pendingState.blendMode = BlendMode::Alpha;
        m_pendingState.faceCulling = false;     // Render both sides

        flushState();
    }

    void GLRenderer::beginSkyboxPass()
    {
        // Save current state
        m_stateStack.push_back(m_currentState);

        // Configure for skybox rendering
        m_pendingState.depthTest = true;
        m_pendingState.depthFunc = DepthFunc::LessOrEqual;  // Render at max depth
        m_pendingState.depthWrite = true;
        m_pendingState.blending = false;
        m_pendingState.faceCulling = false;
        m_pendingState.cullMode = CullMode::Front;  // Render inside of cube

        flushState();
    }

    void GLRenderer::beginPostProcessPass()
    {
        // Save current state
        m_stateStack.push_back(m_currentState);

        // Configure for post-processing (2D overlay)
        m_pendingState.depthTest = false;  // No depth for full-screen quad
        m_pendingState.blending = false;
        m_pendingState.faceCulling = false;

        flushState();
    }

    void GLRenderer::endPass()
    {
        if (m_stateStack.empty())
        {
            LOG_WARN("endPass() called without matching beginPass()");
            return;
        }

        // Restore previous state
        m_pendingState = m_stateStack.back();
        m_stateStack.pop_back();

        flushState();
    }

    // === STATISTICS ===

    void GLRenderer::resetStats()
    {
        m_stateChanges = 0;
        m_stateChangesSaved = 0;
    }

    // === FLUSH STATE (Core Logic) ===

    void GLRenderer::flushState()
    {
        // Depth test
        if (m_pendingState.depthTest != m_currentState.depthTest)
        {
            if (m_pendingState.depthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            m_currentState.depthTest = m_pendingState.depthTest;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Depth write
        if (m_pendingState.depthWrite != m_currentState.depthWrite)
        {
            glDepthMask(m_pendingState.depthWrite ? GL_TRUE : GL_FALSE);
            m_currentState.depthWrite = m_pendingState.depthWrite;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Depth function
        if (m_pendingState.depthFunc != m_currentState.depthFunc)
        {
            glDepthFunc(toGLDepthFunc(m_pendingState.depthFunc));
            m_currentState.depthFunc = m_pendingState.depthFunc;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Blending
        if (m_pendingState.blending != m_currentState.blending)
        {
            if (m_pendingState.blending)
            {
                glEnable(GL_BLEND);

                // Set blend function based on mode
                switch (m_pendingState.blendMode)
                {
                case BlendMode::Alpha:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case BlendMode::Additive:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    break;
                case BlendMode::Multiply:
                    glBlendFunc(GL_DST_COLOR, GL_ZERO);
                    break;
                case BlendMode::Premultiplied:
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                }
            }
            else
            {
                glDisable(GL_BLEND);
            }

            m_currentState.blending = m_pendingState.blending;
            m_currentState.blendMode = m_pendingState.blendMode;
            m_stateChanges++;
        }
        else if (m_pendingState.blending &&
            m_pendingState.blendMode != m_currentState.blendMode)
        {
            // Blending enabled, but mode changed
            switch (m_pendingState.blendMode)
            {
            case BlendMode::Alpha:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::Additive:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            case BlendMode::Multiply:
                glBlendFunc(GL_DST_COLOR, GL_ZERO);
                break;
            case BlendMode::Premultiplied:
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }

            m_currentState.blendMode = m_pendingState.blendMode;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Face culling
        if (m_pendingState.faceCulling != m_currentState.faceCulling)
        {
            if (m_pendingState.faceCulling)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(toGLCullMode(m_pendingState.cullMode));
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }

            m_currentState.faceCulling = m_pendingState.faceCulling;
            m_currentState.cullMode = m_pendingState.cullMode;
            m_stateChanges++;
        }
        else if (m_pendingState.faceCulling &&
            m_pendingState.cullMode != m_currentState.cullMode)
        {
            glCullFace(toGLCullMode(m_pendingState.cullMode));
            m_currentState.cullMode = m_pendingState.cullMode;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Front face
        if (m_pendingState.frontFace != m_currentState.frontFace)
        {
            glFrontFace(m_pendingState.frontFace == FrontFace::CCW ? GL_CCW : GL_CW);
            m_currentState.frontFace = m_pendingState.frontFace;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Polygon mode
        if (m_pendingState.polygonMode != m_currentState.polygonMode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, toGLPolygonMode(m_pendingState.polygonMode));
            m_currentState.polygonMode = m_pendingState.polygonMode;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Line width
        if (m_pendingState.lineWidth != m_currentState.lineWidth)
        {
            glLineWidth(m_pendingState.lineWidth);
            m_currentState.lineWidth = m_pendingState.lineWidth;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // MSAA
        if (m_pendingState.msaa != m_currentState.msaa)
        {
            if (m_pendingState.msaa)
                glEnable(GL_MULTISAMPLE);
            else
                glDisable(GL_MULTISAMPLE);

            m_currentState.msaa = m_pendingState.msaa;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Alpha to coverage
        if (m_pendingState.alphaToCoverage != m_currentState.alphaToCoverage)
        {
            if (m_pendingState.alphaToCoverage)
                glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            else
                glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

            m_currentState.alphaToCoverage = m_pendingState.alphaToCoverage;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }

        // Viewport
        if (m_pendingState.viewportX != m_currentState.viewportX ||
            m_pendingState.viewportY != m_currentState.viewportY ||
            m_pendingState.viewportWidth != m_currentState.viewportWidth ||
            m_pendingState.viewportHeight != m_currentState.viewportHeight)
        {
            glViewport(m_pendingState.viewportX, m_pendingState.viewportY,
                m_pendingState.viewportWidth, m_pendingState.viewportHeight);

            m_currentState.viewportX = m_pendingState.viewportX;
            m_currentState.viewportY = m_pendingState.viewportY;
            m_currentState.viewportWidth = m_pendingState.viewportWidth;
            m_currentState.viewportHeight = m_pendingState.viewportHeight;
            m_stateChanges++;
        }
        else
        {
            m_stateChangesSaved++;
        }
    }

    // === HELPER CONVERSIONS ===

    unsigned int GLRenderer::toGLDepthFunc(DepthFunc func)
    {
        switch (func)
        {
        case DepthFunc::Less:        return GL_LESS;
        case DepthFunc::LessOrEqual: return GL_LEQUAL;
        case DepthFunc::Equal:       return GL_EQUAL;
        case DepthFunc::Greater:     return GL_GREATER;
        case DepthFunc::Always:      return GL_ALWAYS;
        case DepthFunc::Never:       return GL_NEVER;
        default:                     return GL_LESS;
        }
    }

    unsigned int GLRenderer::toGLCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::Back:  return GL_BACK;
        case CullMode::Front: return GL_FRONT;
        case CullMode::None:  return GL_BACK;  // Fallback (culling will be disabled)
        default:              return GL_BACK;
        }
    }

    unsigned int GLRenderer::toGLPolygonMode(PolygonMode mode)
    {
        switch (mode)
        {
        case PolygonMode::Fill:  return GL_FILL;
        case PolygonMode::Line:  return GL_LINE;
        case PolygonMode::Point: return GL_POINT;
        default:                 return GL_FILL;
        }
    }

}