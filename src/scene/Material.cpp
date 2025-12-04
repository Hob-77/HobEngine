#include "scene/Material.h"
#include "core/Logger.h"

namespace Engine
{
    void Material::bind(IShader& shader) const  // Changed parameter type
    {
        // Bind material properties
        shader.setUniform("u_Material_Ambient", ambient);
        shader.setUniform("u_Material_Specular", specular);
        shader.setUniform("u_Material_Shininess", shininess);

        // Texture slot 0: Diffuse/Albedo
        if (m_diffuseMap)
        {
            m_diffuseMap->bind(0);
            shader.setUniform("u_Material_DiffuseMap", 0);
            shader.setUniform("u_Material_HasDiffuseMap", true);
        }
        else
        {
            shader.setUniform("u_Material_Diffuse", diffuse);
            shader.setUniform("u_Material_HasDiffuseMap", false);
        }

        // Texture slot 1: Specular
        if (m_specularMap)
        {
            m_specularMap->bind(1);
            shader.setUniform("u_Material_SpecularMap", 1);
            shader.setUniform("u_Material_HasSpecularMap", true);
        }
        else
        {
            shader.setUniform("u_Material_HasSpecularMap", false);
        }

        // Texture slot 2: Normal map
        if (m_normalMap)
        {
            m_normalMap->bind(2);
            shader.setUniform("u_Material_NormalMap", 2);
            shader.setUniform("u_Material_HasNormalMap", true);
        }
        else
        {
            shader.setUniform("u_Material_HasNormalMap", false);
        }

        // Texture slot 3: Emissive
        if (m_emissiveMap)
        {
            m_emissiveMap->bind(3);
            shader.setUniform("u_Material_EmissiveMap", 3);
            shader.setUniform("u_Material_HasEmissiveMap", true);
        }
        else
        {
            shader.setUniform("u_Material_HasEmissiveMap", false);
        }

        // Alpha for transparency
        shader.setUniform("u_Material_Alpha", alpha);
    }
}