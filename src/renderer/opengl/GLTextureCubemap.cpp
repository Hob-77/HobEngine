#include "renderer/opengl/GLTextureCubemap.h"
#include "core/Logger.h"
#include "stb_image.h"

namespace Engine
{
    GLTextureCubemap::GLTextureCubemap(const std::array<std::string, 6>& faces)
    {
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

        // Face order: +X, -X, +Y, -Y, +Z, -Z
        const char* faceNames[] = { "right", "left", "top", "bottom", "front", "back" };

        bool loadSuccess = true;

        for (int i = 0; i < 6; i++)
        {
            int width, height, channels;

            // Flip is OFF for cubemaps (they're already correctly oriented)
            stbi_set_flip_vertically_on_load(false);

            unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &channels, 0);

            if (data)
            {
                GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;

                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,  // Face target
                    0,                                    // Mipmap level
                    format,                               // Internal format
                    width,                                // Width
                    height,                               // Height
                    0,                                    // Border (must be 0)
                    format,                               // Data format
                    GL_UNSIGNED_BYTE,                     // Data type
                    data                                  // Pixel data
                );

                stbi_image_free(data);

                // Store dimensions from first face
                if (i == 0)
                {
                    m_width = width;
                    m_height = height;
                    m_channels = channels;
                }

                LOG_INFO("Loaded cubemap face {}: {} ({}x{})",
                    faceNames[i], faces[i], width, height);
            }
            else
            {
                LOG_ERROR("Failed to load cubemap face {}: {}", faceNames[i], faces[i]);
                LOG_ERROR("STB Error: {}", stbi_failure_reason());
                loadSuccess = false;
                break;
            }
        }

        if (!loadSuccess)
        {
            LOG_WARN("Creating fallback cubemap (colored faces)");
            glDeleteTextures(1, &m_textureID);
            createFallbackCubemap();
            return;
        }

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // CRITICAL: Enable seamless cubemap filtering (removes seams)
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        LOG_INFO("Cubemap created successfully (ID: {})", m_textureID);
    }

    void GLTextureCubemap::createFallbackCubemap()
    {
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

        m_width = 2;
        m_height = 2;
        m_channels = 3;
        m_isFallback = true;

        // Colored faces for debugging (each face is different color)
        unsigned char faceColors[6][12] = {
            {255,   0,   0,  255,   0,   0,  255,   0,   0,  255,   0,   0}, // +X Red
            {  0, 255,   0,    0, 255,   0,    0, 255,   0,    0, 255,   0}, // -X Green
            {  0,   0, 255,    0,   0, 255,    0,   0, 255,    0,   0, 255}, // +Y Blue
            {255, 255,   0,  255, 255,   0,  255, 255,   0,  255, 255,   0}, // -Y Yellow
            {255,   0, 255,  255,   0, 255,  255,   0, 255,  255,   0, 255}, // +Z Magenta
            {  0, 255, 255,    0, 255, 255,    0, 255, 255,    0, 255, 255}  // -Z Cyan
        };

        for (int i = 0; i < 6; i++)
        {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                GL_RGB,
                2, 2,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                faceColors[i]
            );
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        LOG_INFO("Fallback cubemap created (colored faces)");
    }

    GLTextureCubemap::~GLTextureCubemap()
    {
        if (m_textureID != 0)
        {
            glDeleteTextures(1, &m_textureID);
            LOG_INFO("Cubemap destroyed (ID: {})", m_textureID);
            m_textureID = 0;
        }
    }

    GLTextureCubemap::GLTextureCubemap(GLTextureCubemap&& other) noexcept
        : m_textureID(other.m_textureID)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_channels(other.m_channels)
        , m_isFallback(other.m_isFallback)
    {
        other.m_textureID = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
        other.m_isFallback = false;
    }

    GLTextureCubemap& GLTextureCubemap::operator=(GLTextureCubemap&& other) noexcept
    {
        if (this != &other)
        {
            if (m_textureID != 0)
            {
                glDeleteTextures(1, &m_textureID);
            }

            m_textureID = other.m_textureID;
            m_width = other.m_width;
            m_height = other.m_height;
            m_channels = other.m_channels;
            m_isFallback = other.m_isFallback;

            other.m_textureID = 0;
            other.m_width = 0;
            other.m_height = 0;
            other.m_channels = 0;
            other.m_isFallback = false;
        }
        return *this;
    }

    void GLTextureCubemap::bind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    }

    void GLTextureCubemap::unbind() const
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
}