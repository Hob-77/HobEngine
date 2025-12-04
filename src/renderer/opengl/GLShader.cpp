#include "renderer/opengl/GLShader.h"
#include "core/Logger.h"
#include "core/Error.h"
#include "core/FileUtils.h"
#include <glm/gtc/type_ptr.hpp>

namespace Engine
{
	// Static factory method: Load from files
	GLShader GLShader::loadFromFiles(const char* vertexPath, const char* fragmentPath)
	{
		LOG_INFO("Loading shaders from files:");
		LOG_INFO(" Vertex: {}", vertexPath);
		LOG_INFO(" Fragment: {}", fragmentPath);

		// Read files
		std::string vertexSource = FileUtils::readFile(vertexPath);
		std::string fragmentSource = FileUtils::readFile(fragmentPath);

		// Construct shader from sources
		return GLShader(vertexSource.c_str(), fragmentSource.c_str());
	}

	// Constructor: Compile from source strings
	GLShader::GLShader(const char* vertexSource, const char* fragmentSource)
	{
		LOG_INFO("Creating OpenGL shader program");

		// Compile vertex shader
		GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);

		// Compile fragment shader
		GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

		// Link both shaders into a program
		m_program = linkProgram(vertexShader, fragmentShader);

		// Clean up individual shaders (not needed after linking)
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		LOG_INFO("OpenGL shader program created successfully (ID: {})", m_program);
	}

	GLuint GLShader::compileShader(const char* source, GLenum type)
	{
		// Determine shader type for logging
		const char* typeStr = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
		LOG_INFO("Compiling {} shader", typeStr);

		// Create shader object
		GLuint shader = glCreateShader(type);

		// Attach source code to shader object
		glShaderSource(shader, 1, &source, nullptr);

		// Compile the shader
		glCompileShader(shader);

		// Check for compilation errors
		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			// Get error message from OpenGL
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, nullptr, infoLog);

			// Log the error
			LOG_ERROR("{} shader compilation failed:", typeStr);
			LOG_ERROR("{}", infoLog);

			// Clean up and assert
			glDeleteShader(shader);
			ENGINE_ASSERT(false, "Shader compilation failed");

			return 0; // Never reached
		}

		LOG_INFO("{} shader compiled successfully", typeStr);
		return shader;
	}

	GLuint GLShader::linkProgram(GLuint vertexShader, GLuint fragmentShader)
	{
		LOG_INFO("Linking shader program");

		// Create program object
		GLuint program = glCreateProgram();

		// Attach both shaders to program
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);

		// Link the program
		glLinkProgram(program);

		// Check for link errors
		GLint success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);

		if (!success)
		{
			// Get error message
			char infoLog[512];
			glGetProgramInfoLog(program, 512, nullptr, infoLog);

			// Log error
			LOG_ERROR("Shader program linking failed:");
			LOG_ERROR("{}", infoLog);

			// Clean up
			glDeleteProgram(program);
			ENGINE_ASSERT(false, "Shader linking failed");

			return 0;
		}

		LOG_INFO("Shader program linked successfully");
		return program;
	}

	GLShader::~GLShader()
	{
		if (m_program != 0)
		{
			LOG_INFO("Deleting OpenGL shader program (ID: {})", m_program);
			glDeleteProgram(m_program);
			m_program = 0;
		}
	}

	GLShader::GLShader(GLShader&& other) noexcept
		: m_program(other.m_program)
		, m_uniformCache(std::move(other.m_uniformCache))
	{
		// Steal resources from other
		other.m_program = 0; // Leave other in valid but empty state
	}

	GLShader& GLShader::operator=(GLShader&& other) noexcept
	{
		if (this != &other)
		{
			// Delete our current program
			if (m_program != 0)
			{
				glDeleteProgram(m_program);
			}

			// Steal from other
			m_program = other.m_program;
			m_uniformCache = std::move(other.m_uniformCache);

			// Leave other empty
			other.m_program = 0;
		}

		return *this;
	}

	void GLShader::bind() const
	{
		glUseProgram(m_program);
	}

	void GLShader::unbind() const
	{
		glUseProgram(0);
	}

	GLint GLShader::getUniformLocation(const char* name) const
	{
		// Check cache first
		auto it = m_uniformCache.find(name);
		if (it != m_uniformCache.end())
		{
			return it->second; // Found in cache
		}

		// Not in cache, query OpenGL
		GLint location = glGetUniformLocation(m_program, name);

		if (location == -1)
		{
			LOG_WARN("Uniform '{}' not found in shader", name);
		}

		// Cache the result (even if -1)
		m_uniformCache[name] = location;

		return location;
	}

	void GLShader::setUniform(const char* name, bool value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform1i(location, value ? 1 : 0); // Convert bool to int explicitly
		}
	}

	void GLShader::setUniform(const char* name, int value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform1i(location, value);
		}
	}

	void GLShader::setUniform(const char* name, float value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform1f(location, value);
		}
	}

	void GLShader::setUniform(const char* name, const vec2& value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform2f(location, value.x, value.y);
		}
	}

	void GLShader::setUniform(const char* name, const vec3& value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform3f(location, value.x, value.y, value.z);
		}
	}

	void GLShader::setUniform(const char* name, const vec4& value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniform4f(location, value.x, value.y, value.z, value.w);
		}
	}

	void GLShader::setUniform(const char* name, const mat4& value)
	{
		GLint location = getUniformLocation(name);
		if (location != -1)
		{
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
		}
	}
}