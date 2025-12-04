#include "renderer/opengl/GLTexture.h"
#include "core/Logger.h"
#include "core/Error.h"
#include "stb_image.h"

namespace Engine
{
	// Helper function to convert enums to OpenGL constants
	static GLenum toGLWrap(TextureWrap wrap)
	{
		switch (wrap)
		{
		case TextureWrap::Repeat:         return GL_REPEAT;
		case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
		case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
		case TextureWrap::ClampToBorder:  return GL_CLAMP_TO_BORDER;
		default: return GL_REPEAT;
		}
	}

	static GLenum toGLFilter(TextureFilter filter)
	{
		switch (filter)
		{
		case TextureFilter::Nearest:              return GL_NEAREST;
		case TextureFilter::Linear:               return GL_LINEAR;
		case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
		case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
		case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
		case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
		default: return GL_LINEAR;
		}
	}

	// Default constructor (uses default parameters)
	GLTexture::GLTexture(const char* filepath) : GLTexture(filepath, Parameters{})
	{

	}

	// Main constructor with parameters + fallback texture
	GLTexture::GLTexture(const char* filepath, const Parameters& params) : m_filepath(filepath)
	{
		LOG_INFO("Loading OpenGL texture: {}", filepath);

		// Tell stb_image to flip images vertically (OpenGL expects origin at bottom)
		stbi_set_flip_vertically_on_load(true);

		// Load image data
		unsigned char* data = stbi_load(filepath, &m_width, &m_height, &m_channels, 0);

		if (!data)
		{
			LOG_ERROR("Failed to load texture: {}", filepath);
			LOG_ERROR("STB Error: {}", stbi_failure_reason());
			LOG_WARN("Creating fallback magenta checkerboard texture");

			// Create fallback texture instead of crashing
			createFallbackTexture(params);
			return;
		}

		LOG_INFO("Texture loaded: {}x{} with {} channels", m_width, m_height, m_channels);

		// Determine OpenGL format based on channels
		GLenum internalFormat = 0;
		GLenum dataFormat = 0;

		if (m_channels == 1)
		{
			internalFormat = GL_R8;
			dataFormat = GL_RED;
		}
		else if (m_channels == 3)
		{
			internalFormat = GL_RGB8;
			dataFormat = GL_RGB;
		}
		else if (m_channels == 4)
		{
			internalFormat = GL_RGBA8;
			dataFormat = GL_RGBA;
		}
		else
		{
			LOG_ERROR("Unsupported channel count: {}", m_channels);
			stbi_image_free(data);
			LOG_WARN("Creating fallback texture due to unsupported format");
			createFallbackTexture(params);
			return;
		}

		// Create OpenGL texture
		glGenTextures(1, &m_textureID);
		glBindTexture(GL_TEXTURE_2D, m_textureID);

		// Set texture parameters (using custom parameters)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap(params.wrapS));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap(params.wrapT));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, toGLFilter(params.minFilter));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toGLFilter(params.magFilter));

		// Upload texture data to GPU
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			internalFormat,
			m_width,
			m_height,
			0,
			dataFormat,
			GL_UNSIGNED_BYTE,
			data
		);

		// Generate mipmaps if requested
		if (params.generateMipmaps)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			LOG_INFO("Generated mipmaps for texture");
		}

		// Anisotropic Filtering
		if (params.anisotropy > 0)
		{
			// Validate to allowed values only
			int validAF = 0;
			if (params.anisotropy >= 16)
			{
				validAF = 16;
			}
			else if (params.anisotropy >= 8)
			{
				validAF = 8;
			}
			else if (params.anisotropy >= 4)
			{
				validAF = 4;
			}
			else if (params.anisotropy >= 2)
			{
				validAF = 2;
			}
			else
			{
				validAF = 0; // Disable AF
			}

			if (validAF > 0)
			{
				// Query hardware maximum
				float maxAniso = 0.0f;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);

				// Clamp to hardware limit
				float finalAF = std::min(static_cast<float>(validAF), maxAniso);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, finalAF);

				LOG_TRACE("Texture '{}': AF {}x (requested: {}, hw max: {}x)", filepath, finalAF, params.anisotropy, maxAniso);
			}
			else
			{
				LOG_WARN("Texture '{}': Invalid AF value {} (must be 2, 4, 8, or 16)", filepath, params.anisotropy);
			}
		}

		// Free CPU memory
		stbi_image_free(data);

		// Unbind
		glBindTexture(GL_TEXTURE_2D, 0);

		LOG_INFO("OpenGL texture created successfully (ID: {})", m_textureID);
	}

	void GLTexture::createFallbackTexture(const Parameters& params)
	{
		m_width = 2;
		m_height = 2;
		m_channels = 3;
		m_isFallback = true;

		// Magenta/black checkerboard pattern (easy to spot missing textures)
		unsigned char fallbackData[] = {
			255, 0, 255,  0, 0, 0,      // Top row: magenta, black
			0, 0, 0,      255, 0, 255   // Bottom row: black, magenta
		};

		// Create OpenGL texture
		glGenTextures(1, &m_textureID);
		glBindTexture(GL_TEXTURE_2D, m_textureID);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap(params.wrapS));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap(params.wrapT));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Use NEAREST for crisp checkerboard
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Upload fallback data
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB8,
			2,
			2,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			fallbackData
		);

		// Don't generate mipmaps for fallback (it's only 2x2)

		// Unbind
		glBindTexture(GL_TEXTURE_2D, 0);

		LOG_INFO("Fallback OpenGL texture created (ID: {})", m_textureID);
	}

	GLTexture::~GLTexture()
	{
		if (m_textureID != 0)
		{
			LOG_INFO("Deleting OpenGL texture (ID: {})", m_textureID);
			glDeleteTextures(1, &m_textureID);
			m_textureID = 0;
		}
	}

	GLTexture::GLTexture(GLTexture&& other) noexcept
		: m_textureID(other.m_textureID)
		, m_width(other.m_width)
		, m_height(other.m_height)
		, m_channels(other.m_channels)
		, m_filepath(std::move(other.m_filepath))
		, m_isFallback(other.m_isFallback)
	{
		other.m_textureID = 0;
		other.m_width = 0;
		other.m_height = 0;
		other.m_channels = 0;
		other.m_isFallback = false;
	}

	GLTexture& GLTexture::operator=(GLTexture&& other) noexcept
	{
		if (this != &other)
		{
			// Delete current texture
			if (m_textureID != 0)
			{
				glDeleteTextures(1, &m_textureID);
			}

			// Steal from other
			m_textureID = other.m_textureID;
			m_width = other.m_width;
			m_height = other.m_height;
			m_channels = other.m_channels;
			m_filepath = std::move(other.m_filepath);
			m_isFallback = other.m_isFallback;

			// Leave other empty
			other.m_textureID = 0;
			other.m_width = 0;
			other.m_height = 0;
			other.m_channels = 0;
			other.m_isFallback = false;
		}

		return *this;
	}

	void GLTexture::bind(uint32_t slot) const
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_textureID);
	}

	void GLTexture::unbind() const
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}