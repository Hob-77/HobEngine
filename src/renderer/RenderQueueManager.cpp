#include "renderer/RenderQueueManager.h"
#include "scene/SceneObject.h"
#include "scene/Material.h"
#include "core/Error.h"

namespace Engine
{
    void RenderQueueManager::submit(SceneObject* object, const Material& material, float distance)
    {
        ENGINE_ASSERT(object != nullptr, "Cannot submit null object to render queue manager");

        Material* materialPtr = const_cast<Material*>(&material);

        if (material.isTransparent)
        {
            m_transparentQueue.submit(object, materialPtr, distance);
        }
        else
        {
            m_opaqueQueue.submit(object, materialPtr, 0.0f);
        }
    }

    void RenderQueueManager::sort()
    {
        m_opaqueQueue.sort(true);
        m_transparentQueue.sort(false);
    }

    void RenderQueueManager::render(IShader& shader, IRenderer& renderer)
    {

        // Phase 1: Render opaque objects
        renderer.beginOpaquePass();
        m_opaqueQueue.execute(shader);
        renderer.endPass();

        // Phase 2: Render transparent objects
        renderer.beginTransparentPass();
        m_transparentQueue.execute(shader);
        renderer.endPass();
    }

    void RenderQueueManager::clear()
    {
        m_opaqueQueue.clear();
        m_transparentQueue.clear();
    }
}