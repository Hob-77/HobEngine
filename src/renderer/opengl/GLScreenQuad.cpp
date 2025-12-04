#include "renderer/opengl/GLScreenQuad.h"
#include "core/Logger.h"

namespace Engine
{
    GLScreenQuad::GLScreenQuad()
    {
        // Vertex data: Position (XY) + TexCoords (UV)
        float vertices[] = {
            // Positions        TexCoords
            -1.0f, -1.0f,       0.0f, 0.0f,  // Bottom-left
             1.0f, -1.0f,       1.0f, 0.0f,  // Bottom-right
             1.0f,  1.0f,       1.0f, 1.0f,  // Top-right
            -1.0f,  1.0f,       0.0f, 1.0f   // Top-left
        };

        // Indices (two triangles)
        unsigned int indices[] = {
            0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
            2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
        };

        // Generate VAO
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        // Generate and upload VBO
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Generate and upload EBO
        glGenBuffers(1, &m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Configure vertex attributes
        // Location 0: Position (vec2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        // Location 1: TexCoords (vec2)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        // Unbind
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        LOG_INFO("GLScreenQuad created (VAO: {}, VBO: {}, EBO: {})", m_vao, m_vbo, m_ebo);
    }

    GLScreenQuad::~GLScreenQuad()
    {
        if (m_ebo != 0)
        {
            glDeleteBuffers(1, &m_ebo);
            m_ebo = 0;
        }

        if (m_vbo != 0)
        {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }

        if (m_vao != 0)
        {
            glDeleteVertexArrays(1, &m_vao);
            LOG_INFO("GLScreenQuad destroyed (VAO: {})", m_vao);
            m_vao = 0;
        }
    }

    GLScreenQuad::GLScreenQuad(GLScreenQuad&& other) noexcept
        : m_vao(other.m_vao)
        , m_vbo(other.m_vbo)
        , m_ebo(other.m_ebo)
    {
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
    }

    GLScreenQuad& GLScreenQuad::operator=(GLScreenQuad&& other) noexcept
    {
        if (this != &other)
        {
            // Delete current resources
            if (m_ebo != 0) glDeleteBuffers(1, &m_ebo);
            if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
            if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);

            // Steal from other
            m_vao = other.m_vao;
            m_vbo = other.m_vbo;
            m_ebo = other.m_ebo;

            // Leave other empty
            other.m_vao = 0;
            other.m_vbo = 0;
            other.m_ebo = 0;
        }
        return *this;
    }

    void GLScreenQuad::render() const
    {
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}