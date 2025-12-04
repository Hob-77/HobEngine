#include "scene/Transform.h"

namespace Engine
{
    const mat4& Transform::getModelMatrix() const
    {
        // Recompute if transform modified or explicitly marked dirty
        if (m_modelDirty ||
            m_cachedPosition != position ||
            m_cachedRotation != rotation ||
            m_cachedScale != scale)
        {
            m_cachedModelMatrix = computeModelMatrix();

            // Store current values for next comparison
            m_cachedPosition = position;
            m_cachedRotation = rotation;
            m_cachedScale = scale;
            m_modelDirty = false;
        }

        return m_cachedModelMatrix;
    }

    const mat4& Transform::getRotationMatrix() const
    {
        // Recompute if rotation modified or explicitly marked dirty
        if (m_rotationDirty || m_cachedRotation != rotation)
        {
            m_cachedRotationMatrix = computeRotationMatrix();
            m_cachedRotation = rotation;
            m_rotationDirty = false;
        }

        return m_cachedRotationMatrix;
    }

    mat4 Transform::computeModelMatrix() const
    {
        mat4 model = mat4(1.0f);

        // Apply transformations in TRS order
        model = translate(model, position);

        // Rotate: Yaw (Y) -> Pitch (X) -> Roll (Z)
        if (rotation.y != 0.0f)
            model = rotate(model, radians(rotation.y), vec3(0, 1, 0));
        if (rotation.x != 0.0f)
            model = rotate(model, radians(rotation.x), vec3(1, 0, 0));
        if (rotation.z != 0.0f)
            model = rotate(model, radians(rotation.z), vec3(0, 0, 1));

        model = Engine::scale(model, this->scale);

        return model;
    }

    mat4 Transform::computeRotationMatrix() const
    {
        mat4 rotationMat(1.0f);

        // Same rotation order as model matrix: Y -> X -> Z
        if (rotation.y != 0.0f)
            rotationMat = rotate(rotationMat, radians(rotation.y), vec3(0, 1, 0));
        if (rotation.x != 0.0f)
            rotationMat = rotate(rotationMat, radians(rotation.x), vec3(1, 0, 0));
        if (rotation.z != 0.0f)
            rotationMat = rotate(rotationMat, radians(rotation.z), vec3(0, 0, 1));

        return rotationMat;
    }
}