#include "renderer/camera/CameraBase.h"
#include "core/Logger.h"

namespace Engine
{
	CameraBase::CameraBase(float fov, float nearPlane, float farPlane)
		: m_fov(fov)
		, m_near(nearPlane)
		, m_far(farPlane)
		, m_cachedView(1.0f)
		, m_cachedProjection(1.0f)
		, m_cachedViewProjection(1.0f)
		, m_cachedAspect(0.0f)
		, m_dirty(true)
		, m_prevView(1.0f)
		, m_prevProjection(1.0f)
		, m_prevViewProjection(1.0f)
	{
	}

	mat4 CameraBase::getViewMatrix() const
	{
		if (m_dirty)
		{
			// Recompute only view matrix (projection may still be valid)
			m_cachedView = computeViewMatrix();
			// Note: Don't clear dirt flag here, updateCache() handles it
		}
		return m_cachedView;
	}

	mat4 CameraBase::getProjectionMatrix(float aspectRatio) const
	{
		// Check if aspect ratio changed (window resize)
		if (m_dirty || m_cachedAspect != aspectRatio)
		{
			updateCache(aspectRatio);
		}
		return m_cachedProjection;
	}

	mat4 CameraBase::getViewProjectionMatrix(float aspectRatio) const
	{
		// Check if aspect raio changed (window resize)
		if (m_dirty || m_cachedAspect != aspectRatio)
		{
			updateCache(aspectRatio);
		}
		return m_cachedViewProjection;
	}

	void CameraBase::updatePreviousFrame(float aspectRatio)
	{
		// Save current frame matrices as previous
		// This must be called BEFORE camera updates for next frame
		m_prevView = getViewMatrix();
		m_prevProjection = getProjectionMatrix(aspectRatio);
		m_prevViewProjection = getViewProjectionMatrix(aspectRatio);

		/*
		LOG_TRACE("Camera previous frame matrices updated");
		*/
	}

	void CameraBase::setFOV(float fov)
	{
		if (m_fov != fov)
		{
			m_fov = fov;
			markDirty();
		}
	}

	void CameraBase::setNearPlane(float nearPlane)
	{
		if (m_near != nearPlane)
		{
			m_near = nearPlane;
			markDirty();
		}
	}

	void CameraBase::setFarPlane(float farPlane)
	{
		if (m_far != farPlane)
		{
			m_far = farPlane;
			markDirty();
		}
	}

	void CameraBase::updateCache(float aspectRatio) const
	{
		// Recompute all matrices
		m_cachedView = computeViewMatrix();

	    // Standard: Near plane first, far plane second
	    m_cachedProjection = perspective(radians(m_fov), aspectRatio, m_near, m_far);
		

		m_cachedViewProjection = m_cachedProjection * m_cachedView;

		// Update cached aspect and clear dirty flag
		m_cachedAspect = aspectRatio;
		m_dirty = false;
	}
}