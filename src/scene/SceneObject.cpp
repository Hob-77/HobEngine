#include "scene/SceneObject.h"
#include "core/Logger.h"
#include <cfloat>

namespace Engine
{
    SceneObject::SceneObject(std::shared_ptr<IMesh> mesh, const Material& mat)
    {
        submeshes.push_back({ mesh, mat });
    }

    SceneObject::SceneObject(const std::vector<Submesh>& subs)
        : submeshes(subs)
    {
    }

    void SceneObject::render(IShader& shader) const  // Changed parameter type
    {
        // Set transform once for all submeshes
        shader.setUniform("u_Model", transform.getModelMatrix());

        // Render each submesh with its material
        for (const auto& sub : submeshes)
        {
            sub.material.bind(shader);
            if (sub.mesh)
            {
                sub.mesh->draw();
            }
        }
    }

    IMesh::BoundingSphere SceneObject::getWorldBoundingSphere() const
    {
        if (submeshes.empty())
        {
            return { {0, 0, 0}, 0 };
        }

        // Safety check: Ensure mesh exists
        if (!submeshes[0].mesh)
        {
            LOG_ERROR("SceneObject has null mesh in getWorldBoundingSphere()!");
            return { {0, 0, 0}, 0 };
        }

        auto sphere = submeshes[0].mesh->boundingSphere.toWorld(transform);

        // Validation: Check for garbage data
        if (std::isnan(sphere.center.x) || std::isnan(sphere.center.y) || std::isnan(sphere.center.z) ||
            std::isnan(sphere.radius) || sphere.radius < 0.0f || sphere.radius > 1000.0f)
        {
            LOG_ERROR("Invalid bounding sphere! center=({}, {}, {}), radius={}",
                sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
            // Return a large safe sphere as fallback
            return { transform.position, 10.0f };
        }

        return sphere;
    }

    IMesh::AABB SceneObject::getWorldAABB() const
    {
        if (submeshes.empty())
        {
            return { {0, 0, 0}, {0, 0, 0} };
        }

        // Calculate union of all AABBs
        vec3 min(FLT_MAX);
        vec3 max(-FLT_MAX);

        for (const auto& sub : submeshes)
        {
            if (!sub.mesh) continue;

            auto aabb = sub.mesh->aabb.toWorld(transform);
            min = Engine::min(min, aabb.min);
            max = Engine::max(max, aabb.max);
        }

        return { min, max };
    }

    std::shared_ptr<IMesh> SceneObject::getMesh() const
    {
        return submeshes.empty() ? nullptr : submeshes[0].mesh;
    }
}