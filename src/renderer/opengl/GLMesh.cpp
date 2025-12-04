#include "renderer/opengl/GLMesh.h"
#include "scene/Transform.h"
#include "core/Logger.h"
#include "core/Error.h"

namespace Engine
{
	GLMesh::BoundingSphere GLMesh::BoundingSphere::toWorld(const Transform& transform) const
	{
		vec3 worldCenter = vec3(transform.getModelMatrix() * vec4(center, 1.0f));
		float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
		return { worldCenter, radius * maxScale };
	}

	GLMesh::AABB GLMesh::AABB::toWorld(const Transform& transform) const
	{
		vec3 corners[8] =
		{
			{min.x, min.y, min.z}, {max.x, min.y, min.z},
			{min.x, max.y, min.z}, {max.x, max.y, min.z},
			{min.x, min.y, max.z}, {max.x, min.y, max.z},
			{min.x, max.y, max.z}, {max.x, max.y, max.z}
		};

		vec3 worldMin(FLT_MAX);
		vec3 worldMax(-FLT_MAX);
		mat4 model = transform.getModelMatrix();

		for (auto& corner : corners)
		{
			vec3 worldCorner = vec3(model * vec4(corner, 1.0f));
			worldMin = glm::min(worldMin, worldCorner);
			worldMax = glm::max(worldMax, worldCorner);
		}

		return { worldMin, worldMax };
	}

	GLMesh::GLMesh(const float* vertices, size_t dataSize, uint32_t vertexCount) : m_vertexCount(vertexCount)
	{
		LOG_INFO("Creating mesh ({} vertices, {} floats)", vertexCount, dataSize);

		// Validate input
		ENGINE_ASSERT(vertices != nullptr, "Vertex data is null");
		ENGINE_ASSERT(dataSize > 0, "Vertex data size is 0");
		ENGINE_ASSERT(vertexCount > 0, "Vertex count is 0");

		// Expect 6 floats per vertex (position + color)
		ENGINE_ASSERT(dataSize == vertexCount * 6, "Data size mismatch: expected {} floats, got {}", vertexCount * 6, dataSize);

		// Generate VAO (Vertex Array Object)
		glGenVertexArrays(1, &m_vao);
		if (m_vao == 0)
		{
			LOG_ERROR("Failed to generate VAO");
			ENGINE_ASSERT(false, "VAO generation failed");
			return;
		}
		LOG_INFO("Generated VAO (ID: {})", m_vao);

		// Generate VBO (Vertex Buffer Object)
		glGenBuffers(1, &m_vbo);
		if (m_vbo == 0)
		{
			LOG_ERROR("Failed to generate VBO");
			glDeleteVertexArrays(1, &m_vao); // Cleanup VAO
			ENGINE_ASSERT(false, "VBO generation failed");
			return;
		}
		LOG_INFO("Generated VBO (ID: {})", m_vbo);

		// Bind VAO (all subsequent calls modify this VAO)
		glBindVertexArray(m_vao);

		// Bind VBO and upload data
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

		// Upload vertex data to GPU
		size_t sizeInBytes = dataSize * sizeof(float);
		glBufferData(GL_ARRAY_BUFFER, sizeInBytes, vertices, GL_STATIC_DRAW);

		LOG_INFO("Uploaded {} bytes to VBO", sizeInBytes);

		// Configure vertex attributes (how to interpret VBO data)
		setupVertexAttributes();

		// Unbind (clean state)
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
		glBindVertexArray(0);

		LOG_INFO("Mesh created successfully");
	}

	GLMesh::GLMesh(const float* vertices, size_t vertexDataSize, const uint32_t* indices, size_t indexCount, VertexFormat format)
		: m_indexCount(static_cast<uint32_t>(indexCount))
		, m_useIndices(true)
		, m_format(format)
	{
		// Calculate vertex count based on format
		size_t floatsPerVertex = 0;
		const char* formatName = "";

		switch (format)
		{
		case VertexFormat::PositionColor:
			floatsPerVertex = 6;
			formatName = "colored";
			break;
		case VertexFormat::PositionUV:
			floatsPerVertex = 5;
			formatName = "textured";
			break;
		case VertexFormat::PositionNormalUV:
			floatsPerVertex = 8;
			formatName = "lit";
			break;
		}

		m_vertexCount = static_cast<uint32_t>(vertexDataSize / floatsPerVertex);

		LOG_INFO("Creating indexed mesh ({} vertices, {} indices, {})", m_vertexCount, indexCount, formatName);

		// Validate input
		ENGINE_ASSERT(vertices != nullptr, "Vertex data is null");
		ENGINE_ASSERT(indices != nullptr, "Index data is null");
		ENGINE_ASSERT(vertexDataSize > 0, "Vertex data size is 0");
		ENGINE_ASSERT(indexCount > 0, "Index count is 0");

		// Generate VAO
		glGenVertexArrays(1, &m_vao);
		ENGINE_ASSERT(m_vao != 0, "Failed to generate VAO");
		LOG_INFO("Generated VAO (ID: {})", m_vao);

		// Generate VBO
		glGenBuffers(1, &m_vbo);
		ENGINE_ASSERT(m_vbo != 0, "Failed to generate VBO");
		LOG_INFO("Generated VBO (ID: {})", m_vbo);

		// Generate EBO
		glGenBuffers(1, &m_ebo);
		ENGINE_ASSERT(m_ebo != 0, "Failed to generate EBO");
		LOG_INFO("Generated EBO (ID: {})", m_ebo);

		// Bind VAO
		glBindVertexArray(m_vao);

		// Upload vertex data to VBO
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		size_t vertexBytes = vertexDataSize * sizeof(float);
		glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertices, GL_STATIC_DRAW);
		LOG_INFO("Uploaded {} bytes to VBO", vertexBytes);

		// Upload index data to EBO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
		size_t indexBytes = indexCount * sizeof(uint32_t);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes, indices, GL_STATIC_DRAW);
		LOG_INFO("Uploaded {} bytes to EBO", indexBytes);

		// Configure attributes
		setupVertexAttributes();

		// Unbind VBO (EBO stays bound to VAO)
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		LOG_INFO("Indexed mesh created successfully");
	}

	GLMesh::~GLMesh()
	{
		if (m_ebo != 0)
		{
			LOG_INFO("Deleting EBO (ID: {})", m_ebo);
			glDeleteBuffers(1, &m_ebo);
			m_ebo = 0;
		}

		if (m_vbo != 0)
		{
			LOG_INFO("Deleting VBO (ID: {})", m_vbo);
			glDeleteBuffers(1, &m_vbo);
			m_vbo = 0;
		}

		if (m_vao != 0)
		{
			LOG_INFO("Deleting VAO (ID: {})", m_vao);
			glDeleteVertexArrays(1, &m_vao);
			m_vao = 0;
		}
	}

	GLMesh::GLMesh(GLMesh&& other) noexcept
		: m_vao(other.m_vao),
		  m_vbo(other.m_vbo),
		  m_ebo(other.m_ebo),
		  m_vertexCount(other.m_vertexCount),
		  m_indexCount(other.m_indexCount),
		  m_useIndices(other.m_useIndices),
		  m_format(other.m_format)
	{
		// Steal resources from other
		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
		other.m_vertexCount = 0;
		other.m_indexCount = 0;
		other.m_useIndices = false;
	}

	GLMesh& GLMesh::operator=(GLMesh&& other) noexcept
	{
		if (this != &other)
		{

			// Delete our current resources
			if (m_ebo != 0)
			{
				glDeleteBuffers(1, &m_ebo);
			}
			if (m_vbo != 0)
			{
				glDeleteBuffers(1, &m_vbo);
			}
			if (m_vao != 0)
			{
				glDeleteVertexArrays(1, &m_vao);
			}

			// Steal from other
			m_vao = other.m_vao;
			m_vbo = other.m_vbo;
			m_ebo = other.m_ebo;
			m_vertexCount = other.m_vertexCount;
			m_indexCount = other.m_indexCount;
			m_useIndices = other.m_useIndices;
			m_format = other.m_format;

			// Leave other empty
			other.m_vao = 0;
			other.m_vbo = 0;
			other.m_ebo = 0;
			other.m_vertexCount = 0;
			other.m_indexCount = 0;
			other.m_useIndices = false;
		}

			return *this;
	}

	void GLMesh::draw() const
	{
		// Bind VAO (contains all vertex attribute configuration)
		glBindVertexArray(m_vao);
		if (m_useIndices)
		{
			// Indexed rendering
			glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
		}
		else
		{
			// Non-indexed rendering
		    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
	    }

		glBindVertexArray(0);
	}

	void GLMesh::setupVertexAttributes()
	{
		switch (m_format)
		{
		case VertexFormat::PositionColor:
		{
			// Colored vertex format: Position (3) + Color (3) = 6 floats
			constexpr size_t stride = 6 * sizeof(float);

			// Attribute 0: Position (x, y, z)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
			glEnableVertexAttribArray(0);

			// Attribute 1: Color (r, g, b)
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
				(void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			LOG_INFO("Configured vertex attributes (position + color)");
			break;
		}

		case VertexFormat::PositionUV:
		{
			// Textured vertex format: Position (3) + UV (2) = 5 floats
			constexpr size_t stride = 5 * sizeof(float);

			// Attribute 0: Position (x, y, z)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
			glEnableVertexAttribArray(0);

			// Attribute 1: UV (u, v)
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
				(void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			LOG_INFO("Configured vertex attributes (position + UV)");
			break;
		}

		case VertexFormat::PositionNormalUV:
		{
			// Lit vertex format: Position (3) + Normal (3) + UV (2) = 8 floats
			constexpr size_t stride = 8 * sizeof(float);

			// Attribute 0: Position (x, y, z)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
			glEnableVertexAttribArray(0);

			// Attribute 1: Normal (x, y, z)
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
				(void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			// Attribute 2: UV (u, v)
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
				(void*)(6 * sizeof(float)));
			glEnableVertexAttribArray(2);

			LOG_INFO("Configured vertex attributes (position + normal + UV)");
			break;
		}
		}
	}

	void GLMesh::drawInstanced(uint32_t instanceCount) const
	{
		ENGINE_ASSERT(instanceCount > 0, "Instance count must be > 0");
		ENGINE_ASSERT(m_instanceVBO != 0, "Instance VBO not configured - call setupInstancedRendering() first");

		glBindVertexArray(m_vao);

		if (m_useIndices)
		{
			glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0, instanceCount);
		}
		else
		{
			glDrawArraysInstanced(GL_TRIANGLES, 0, m_vertexCount, instanceCount);
		}

		glBindVertexArray(0);
	}

	void GLMesh::setupInstancedRendering(GLuint instanceVBO, size_t stride)
	{
		ENGINE_ASSERT(instanceVBO != 0, "Instance VBO cannot be 0");
		ENGINE_ASSERT(stride > 0, "Stride must be > 0");

		m_instanceVBO = instanceVBO;

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

		// Configure mat4 instance attribute (takes 4 consecutive vec4 slots)
		// Locations 3, 4, 5, 6 for the 4 rows of the matrix
		for (int i = 0; i < 4; i++)
		{
			GLuint location = 3 + i;
			glEnableVertexAttribArray(location);
			glVertexAttribPointer(
				location,
				4,                              // vec4 (one row of mat4)
				GL_FLOAT,
				GL_FALSE,
				static_cast<GLsizei>(stride),
				(void*)(i * sizeof(vec4))      // Offset for each row
			);
			glVertexAttribDivisor(location, 1); // Advance once per instance
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		LOG_INFO("Configured instanced rendering for mesh (VAO: {}, instance VBO: {})", m_vao, instanceVBO);
	}

}