#include "renderer/InstancedBatch.h"
#include "renderer/opengl/GLMesh.h"
#include "core/Logger.h"
#include "core/Error.h"

namespace Engine
{
    InstancedBatch::InstancedBatch(std::shared_ptr<IMesh> mesh, const Material& material)
        : m_mesh(mesh), m_material(material)
    {
        ENGINE_ASSERT(mesh != nullptr, "Mesh cannot be null");

        // Generate instance VBO
        glGenBuffers(1, &m_instanceVBO);
        LOG_INFO("InstancedBatch created (VBO: {})", m_instanceVBO);

        // Configure mesh for instanced rendering
        auto glMesh = std::dynamic_pointer_cast<GLMesh>(mesh);
        if (glMesh)
        {
            glMesh->setupInstancedRendering(m_instanceVBO, sizeof(mat4));
        }
        else
        {
            LOG_ERROR("Mesh is not a GLMesh - instanced rendering may not work");
        }
    }

    InstancedBatch::~InstancedBatch()
    {
        if (m_instanceVBO != 0)
        {
            glDeleteBuffers(1, &m_instanceVBO);
            LOG_INFO("InstancedBatch destroyed (VBO: {})", m_instanceVBO);
            m_instanceVBO = 0;
        }
    }

    InstancedBatch::InstancedBatch(InstancedBatch&& other) noexcept
        : m_mesh(std::move(other.m_mesh))
        , m_material(std::move(other.m_material))
        , m_transforms(std::move(other.m_transforms))
        , m_instanceVBO(other.m_instanceVBO)
        , m_needsUpload(other.m_needsUpload)
    {
        other.m_instanceVBO = 0;
        other.m_needsUpload = false;
    }

    InstancedBatch& InstancedBatch::operator=(InstancedBatch&& other) noexcept
    {
        if (this != &other)
        {
            // Delete our current VBO
            if (m_instanceVBO != 0)
            {
                glDeleteBuffers(1, &m_instanceVBO);
            }

            // Steal from other
            m_mesh = std::move(other.m_mesh);
            m_material = std::move(other.m_material);
            m_transforms = std::move(other.m_transforms);
            m_instanceVBO = other.m_instanceVBO;
            m_needsUpload = other.m_needsUpload;

            // Leave other empty
            other.m_instanceVBO = 0;
            other.m_needsUpload = false;
        }
        return *this;
    }

    void InstancedBatch::addInstance(const Transform& transform)
    {
        m_transforms.push_back(transform.getModelMatrix());
        m_needsUpload = true;
    }

    void InstancedBatch::clear()
    {
        m_transforms.clear();
        m_needsUpload = true;
    }

    void InstancedBatch::upload()
    {
        if (!m_needsUpload || m_transforms.empty())
        {
            return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            m_transforms.size() * sizeof(mat4),
            m_transforms.data(),
            GL_DYNAMIC_DRAW  // Dynamic: updated every frame
        );
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_needsUpload = false;

        LOG_TRACE("Uploaded {} instance transforms ({} KB)",
            m_transforms.size(),
            (m_transforms.size() * sizeof(mat4)) / 1024);
    }

    void InstancedBatch::render(IShader& shader) const
    {
        if (m_transforms.empty())
        {
            LOG_WARN("InstancedBatch::render() called with 0 instances");
            return;
        }

        ENGINE_ASSERT(!m_needsUpload, "Must call upload() before render()");

        shader.bind();
        m_material.bind(shader);
        m_mesh->drawInstanced(static_cast<uint32_t>(m_transforms.size()));
    }
}