#include "scene/Scene.h"
#include "core/Logger.h"

namespace Engine
{

	void Scene::addLight(const Light& light)
	{
		m_lights.push_back(light);
	}

	void Scene::clear()
	{
		m_objects.clear();
		m_lights.clear();
	}

}