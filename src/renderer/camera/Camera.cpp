#include "renderer/camera/Camera.h"
#include "core/Logger.h"

namespace Engine
{
	Camera::Camera(const vec3& position, const vec3& target,
		float fov, float nearPlane, float farPlane)
		: CameraBase(fov, nearPlane, farPlane)
		, m_position(position)
		, m_target(target)
		, m_up(0.0f, 1.0f, 0.0f)
	{
		LOG_INFO("Camera created at ({:.1f}, {:.1f}, {:.1f}), looking at ({:.1f}, {:.1f}, {:.1f})", position.x, position.y, position.z, target.x, target.y, target.z);
	}

	mat4 Camera::computeViewMatrix() const
	{
		// GLM's lookAt creates view matrix
		// Transforms world space -> camera space
		// Camera ends up at origin, looking down -Z
		return lookAt(m_position, m_target, m_up);
	}

    void Camera::setPosition(const vec3& position)
    {
        m_position = position;
        markDirty();  // Camera moved, invalidate cached matrices
    }

    void Camera::setTarget(const vec3& target)
    {
        m_target = target;
        markDirty();  // Camera orientation changed, invalidate cached matrices
    }

    void Camera::setUpVector(const vec3& up)
    {
        m_up = up;
        markDirty();  // Camera orientation changed, invalidate cached matrices
    }

    vec3 Camera::getForward() const
    {
        // Direction from camera to target
        return normalize(m_target - m_position);
    }

    vec3 Camera::getRight() const
    {
        // Right = forward × up
        // Cross product gives perpendicular vector
        vec3 forward = getForward();
        return normalize(cross(forward, m_up));
    }

    vec3 Camera::getUp() const
    {
        // True up = right × forward
        // Ensures orthogonal basis
        vec3 forward = getForward();
        vec3 right = getRight();
        return normalize(cross(right, forward));
    }
}