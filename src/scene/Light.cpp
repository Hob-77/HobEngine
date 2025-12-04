#include "scene/Light.h"
#include "core/Logger.h"
#include <string>

namespace Engine
{
    void Light::bind(IShader& shader, int index) const
    {
        // Validate index range
        if (index < 0 || index >= 16)
        {
            LOG_WARN("Light index {} out of range [0, 16)", index);
            return;
        }

        // Generate indexed uniform names: u_Lights[0].position, u_Lights[1].color, etc.
        std::string base = "u_Lights[" + std::to_string(index) + "]";

        shader.setUniform((base + ".position").c_str(), position);
        shader.setUniform((base + ".color").c_str(), getEffectiveColor());

        // Log only first frame for debugging (throttled)
        static int logCount = 0;
        if (logCount++ < 3)
        {
            LOG_TRACE("Light[{}] bound: pos=({:.1f}, {:.1f}, {:.1f}), color=({:.2f}, {:.2f}, {:.2f})",
                index,
                position.x, position.y, position.z,
                getEffectiveColor().x, getEffectiveColor().y, getEffectiveColor().z);
        }
    }
}