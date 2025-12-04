#include "renderer/Skybox.h"
#include "renderer/MeshFactory.h"
#include "renderer/ShaderManager.h"
#include "renderer/opengl/GLTextureCubemap.h"
#include "core/Logger.h"

namespace Engine
{
    Skybox::Skybox(const std::array<std::string, 6>& faces, IRenderDevice* renderDevice)
    {
        LOG_INFO("Creating skybox...");

        // Create skybox cube mesh (inverted normals, renders from inside)
        m_mesh = MeshFactory::createSkyboxCube(1.0f);  // Size doesn't matter (infinite distance)

        // Load cubemap texture
        m_cubemap = std::make_shared<GLTextureCubemap>(faces);

        // Load skybox shader
        m_shader = ShaderManager::get().loadShader(
            "skybox",
            "assets/shaders/skybox.vert",
            "assets/shaders/skybox.frag"
        );

        if (!m_shader)
        {
            LOG_ERROR("Failed to load skybox shader!");
        }

        LOG_INFO("Skybox created successfully");
    }

    void Skybox::render(const CameraBase& camera, const Window& window)
    {
        if (!m_shader || !m_mesh || !m_cubemap)
        {
            LOG_ERROR("Skybox missing required components");
            return;
        }

        m_shader->bind();

        // Set view and projection (shader will remove translation)
        m_shader->setUniform("u_View", camera.getViewMatrix());
        m_shader->setUniform("u_Projection", camera.getProjectionMatrix(window.getAspectRatio()));

        // Bind cubemap to slot 0
        m_cubemap->bind(0);
        m_shader->setUniform("u_Skybox", 0);

        // Render cube
        m_mesh->draw();

        m_cubemap->unbind();
        m_shader->unbind();
    }
}