#include "renderer/RenderQueue.h"
#include "scene/SceneObject.h"
#include "scene/Material.h"
#include "core/Logger.h"
#include "core/Error.h"
#include <cmath>

namespace
{
    // Helper function to compare materials by value
    bool materialsEqual(const Engine::Material* a, const Engine::Material* b)
    {
        if (a == nullptr || b == nullptr) return false;

        const float EPSILON = 0.0001f;
        auto eq = [EPSILON](float x, float y) { return std::abs(x - y) < EPSILON; };

        // Check transparency flag first
        if (a->isTransparent != b->isTransparent) return false;

        return eq(a->alpha, b->alpha) &&
            eq(a->diffuse.x, b->diffuse.x) &&
            eq(a->diffuse.y, b->diffuse.y) &&
            eq(a->diffuse.z, b->diffuse.z) &&
            eq(a->specular.x, b->specular.x) &&
            eq(a->specular.y, b->specular.y) &&
            eq(a->specular.z, b->specular.z) &&
            eq(a->shininess, b->shininess);
    }
}

namespace Engine
{
    void RenderQueue::submit(SceneObject* object, Material* material, float distance)
    {
        // Validate inputs at entry point
        ENGINE_ASSERT(object != nullptr, "Cannot submit null object to render queue");
        ENGINE_ASSERT(material != nullptr, "Cannot submit null material to render queue");

        m_commands.push_back({ object, material, distance });
    }

    void RenderQueue::sort(bool frontToBack)
    {
        if (m_commands.empty())
            return;

        // Validate all commands have valid materials before sorting
        for (const auto& cmd : m_commands)
        {
            ENGINE_ASSERT(cmd.material != nullptr, "Cannot sort queue with null material");
            ENGINE_ASSERT(cmd.object != nullptr, "Cannot sort queue with null object");
        }

        if (frontToBack)
        {
            // OPAQUE: Material batching priority
            std::sort(m_commands.begin(), m_commands.end(),
                [](const RenderCommand& a, const RenderCommand& b) {
                    if (!(*a.material < *b.material || *b.material < *a.material))
                    {
                        return a.distanceToCamera < b.distanceToCamera;
                    }
                    return *a.material < *b.material;
                });
        }
        else
        {
            // TRANSPARENT: Distance priority
            std::sort(m_commands.begin(), m_commands.end(),
                [](const RenderCommand& a, const RenderCommand& b) {
                    if (std::abs(a.distanceToCamera - b.distanceToCamera) > 0.01f)
                        return a.distanceToCamera > b.distanceToCamera;
                    return *a.material < *b.material;
                });
        }
    }

    void RenderQueue::execute(IShader& shader)  // Changed parameter type
    {
        if (m_commands.empty())
            return;

        // Reset statistics
        m_materialBindCount = 0;
        m_materialBindsSaved = 0;
        Material* lastMaterial = nullptr;

        // Render all commands in sorted order
        for (const auto& cmd : m_commands)
        {
            // Should never trigger if submit() validated correctly
            ENGINE_ASSERT(cmd.material != nullptr, "RenderCommand has null material");
            ENGINE_ASSERT(cmd.object != nullptr, "RenderCommand has null object");

            // Compare material values using helper function
            if (lastMaterial == nullptr || !materialsEqual(cmd.material, lastMaterial))
            {
                cmd.material->bind(shader);
                lastMaterial = cmd.material;
                m_materialBindCount++;
            }
            else
            {
                // Same material as last draw, skip bind
                m_materialBindsSaved++;
            }

            // Render the object
            cmd.object->render(shader);
        }
    }

    void RenderQueue::clear()
    {
        m_commands.clear();
        m_materialBindCount = 0;
        m_materialBindsSaved = 0;
    }
}