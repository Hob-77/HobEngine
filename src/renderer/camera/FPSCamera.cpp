#include "renderer/camera/FPSCamera.h"
#include "input/Input.h"
#include "core/Logger.h"
#include <algorithm>

namespace Engine
{
    FPSCamera::FPSCamera(const vec3& position, float yaw, float pitch, float fov, float nearPlane, float farPlane)
        : CameraBase(fov, nearPlane, farPlane)
        , m_position(position)
        , m_worldUp(0.0f, 1.0f, 0.0f)
        , m_yaw(yaw)
        , m_pitch(pitch)
    {
        updateCameraVectors();

        LOG_INFO("FPS Camera created at ({:.1f}, {:.1f}, {:.1f}), yaw={:.1f}, pitch={:.1f}", position.x, position.y, position.z, yaw, pitch);
    }

    mat4 FPSCamera::computeViewMatrix() const
    {
        // GLM's lookAt creates view matrix
        // Camera at m_position, looking at m_position + m_front
        return lookAt(m_position, m_position + m_front, m_up);
    }

    void FPSCamera::processKeyboard(float deltaTime)
    {
        // Calculate movement speed (base speed or sprint)
        float velocity = m_settings.walkSpeed * deltaTime;
        if (Input::isKeyPressed(SDLK_LSHIFT))
        {
            velocity *= m_settings.sprintMultiplier;
        }

        // WASD movement
        if (Input::isKeyPressed(SDLK_W))
        {
            m_position += m_front * velocity;
            markDirty();  // Camera moved, invalidate cached matrices
        }
        if (Input::isKeyPressed(SDLK_S))
        {
            m_position -= m_front * velocity;
            markDirty();
        }
        if (Input::isKeyPressed(SDLK_A))
        {
            m_position -= m_right * velocity;
            markDirty();
        }
        if (Input::isKeyPressed(SDLK_D))
        {
            m_position += m_right * velocity;
            markDirty();
        }

        // Vertical movement (fly mode)
        if (m_settings.flyMode)
        {
            if (Input::isKeyPressed(SDLK_SPACE))
            {
                m_position += m_worldUp * velocity;
                markDirty();
            }
            if (Input::isKeyPressed(SDLK_LCTRL))
            {
                m_position -= m_worldUp * velocity;
                markDirty();
            }
        }
    }

    void FPSCamera::processMouseMovement(float deltaX, float deltaY, bool constrainPitch)
    {
        // Apply sensitivity
        deltaX *= m_settings.mouseSensitivity;
        deltaY *= m_settings.mouseSensitivity;

        // Invert Y if requested
        if (m_settings.invertY)
        {
            deltaY = -deltaY;
        }

        // Update Euler angles
        m_yaw += deltaX;
        m_pitch -= deltaY;

        // Constrain pitch to prevent gimbal lock
        if (constrainPitch)
        {
            if (m_pitch > 89.0f)
                m_pitch = 89.0f;
            if (m_pitch < -89.0f)
                m_pitch = -89.0f;
        }

        // Recalculate camera vectors
        updateCameraVectors();
        markDirty();  // Camera rotated, invalidate cached matrices
    }

    void FPSCamera::processMouseScroll(float yOffset)
    {
        // Zoom by adjusting FOV
        float newFOV = getFOV() - yOffset;

        // Clamp FOV to reasonable range
        if (newFOV < 1.0f)
            newFOV = 1.0f;
        if (newFOV > 90.0f)
            newFOV = 90.0f;

        setFOV(newFOV);  // CameraBase::setFOV() calls markDirty() internally
    }

    void FPSCamera::setPosition(const vec3& position)
    {
        m_position = position;
        markDirty();
    }

    void FPSCamera::reset(const vec3& position, float yaw, float pitch)
    {
        m_position = position;
        m_yaw = yaw;
        m_pitch = pitch;

        updateCameraVectors();
        markDirty();

        LOG_INFO("FPS Camera reset to ({}, {}, {}), yaw={}, pitch={}", position.x, position.y, position.z, yaw, pitch);
    }

    void FPSCamera::updateCameraVectors()
    {
        // Calculate forward vector from yaw and pitch
        vec3 forward;
        forward.x = cos(radians(m_yaw)) * cos(radians(m_pitch));
        forward.y = sin(radians(m_pitch));
        forward.z = sin(radians(m_yaw)) * cos(radians(m_pitch));
        m_front = normalize(forward);

        // Calculate right and up vectors
        m_right = normalize(cross(m_front, m_worldUp));
        m_up = normalize(cross(m_right, m_front));
    }
}