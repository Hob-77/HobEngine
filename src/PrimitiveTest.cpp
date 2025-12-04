#include "math/EngineMath.h"
#include "renderer/camera/FPSCamera.h"
#include "renderer/MeshFactory.h"
#include "renderer/opengl/GLDebugRenderer.h"
#include "renderer/ShaderManager.h"
#include "renderer/camera/Camera.h"
#include "renderer/PostProcessManager.h"
#include "renderer/opengl/GLRenderDevice.h"
#include "renderer/AssetManager.h"
#include "renderer/Skybox.h"
#include "core/Application.h"
#include "core/EngineTime.h"
#include "input/Input.h"
#include "scene/Scene.h"
#include "scene/SceneObject.h"
#include <iostream>
#include <optional>

// Add this before the class definition
void logSoakTestStats() {
    static uint64_t totalFrames = 0;
    static float timeAccumulator = 0.0f;

    totalFrames++;

    float deltaTime = Engine::EngineTime::getDeltaTime();
    timeAccumulator += deltaTime;

    // Log every 60 seconds
    if (timeAccumulator >= 60.0f) {
        // Get total runtime
        float totalRuntime = Engine::EngineTime::getTime();

        // Calculate hours and minutes
        int hours = (int)(totalRuntime / 3600.0f);
        int minutes = (int)((totalRuntime - hours * 3600.0f) / 60.0f);

        // Get current FPS from EngineTime
        uint32_t currentFPS = Engine::EngineTime::getFPS();

        // Calculate average FPS
        float avgFPS = totalFrames / totalRuntime;

        LOG_TRACE("Soak Test | {}h {}m | Total Frames: {} | Current FPS: {} | Avg FPS: {:.1f}",
            hours, minutes, totalFrames, currentFPS, avgFPS);

        // Reset accumulator for next interval
        timeAccumulator = 0.0f;
    }
}

class CompletePrimitiveShowcaseApp : public Engine::Application
{
public:
    CompletePrimitiveShowcaseApp()
        : Application(createConfig())
    {
    }

    void onInitialize() override
    {
        LOG_INFO("=== COMPLETE PRIMITIVE SHOWCASE ===");
        LOG_INFO("Demonstrating ALL 10 MeshFactory primitives:");
        LOG_INFO("  Row 1: Cube, Sphere, Cylinder, Plane, Quad");
        LOG_INFO("  Row 2: Cone, Pyramid, Capsule, Torus, Skybox");

        m_shader = Engine::ShaderManager::get().loadShader(
            "basic",
            "assets/shaders/basic.vert",
            "assets/shaders/basic.frag"
        );

        m_fpsCamera.emplace(
            Engine::vec3(0.0f, 4.0f, 15.0f),
            -90.0f,
            -15.0f
        );

        auto& settings = m_fpsCamera->getMovementSettings();
        settings.walkSpeed = 8.0f;
        settings.sprintMultiplier = 2.5f;
        settings.flyMode = true;

        Engine::vec3 orbitPos(
            Engine::cos(m_orbitAngle) * m_orbitDistance,
            m_orbitHeight,
            Engine::sin(m_orbitAngle) * m_orbitDistance
        );

        m_orbitCamera.emplace(
            orbitPos,
            Engine::vec3(0, 2, 0),
            45.0f,
            0.1f,
            1000.0f
        );

        m_cameraMode = CameraMode::Orbit;

        Engine::Input::setMouseCursorLocked(false);
        Engine::Input::setMouseCursorVisible(true);
        m_mouseLocked = false;

        Engine::Material redMat, orangeMat, yellowMat, greenMat, cyanMat;

        redMat.diffuse = Engine::vec3(1.0f, 0.2f, 0.2f);
        redMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        redMat.shininess = 32.0f;

        orangeMat.diffuse = Engine::vec3(1.0f, 0.6f, 0.2f);
        orangeMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        orangeMat.shininess = 64.0f;

        yellowMat.diffuse = Engine::vec3(1.0f, 1.0f, 0.2f);
        yellowMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        yellowMat.shininess = 32.0f;

        greenMat.diffuse = Engine::vec3(0.2f, 1.0f, 0.2f);
        greenMat.specular = Engine::vec3(0.5f, 0.5f, 0.5f);
        greenMat.shininess = 8.0f;

        cyanMat.diffuse = Engine::vec3(0.2f, 1.0f, 1.0f);
        cyanMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        cyanMat.shininess = 32.0f;

        Engine::Material blueMat, purpleMat, magentaMat, pinkMat, whiteMat;

        blueMat.diffuse = Engine::vec3(0.2f, 0.4f, 1.0f);
        blueMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        blueMat.shininess = 32.0f;

        purpleMat.diffuse = Engine::vec3(0.6f, 0.2f, 1.0f);
        purpleMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        purpleMat.shininess = 32.0f;

        magentaMat.diffuse = Engine::vec3(1.0f, 0.2f, 1.0f);
        magentaMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        magentaMat.shininess = 32.0f;

        pinkMat.diffuse = Engine::vec3(1.0f, 0.4f, 0.7f);
        pinkMat.specular = Engine::vec3(1.0f, 1.0f, 1.0f);
        pinkMat.shininess = 64.0f;

        whiteMat.diffuse = Engine::vec3(0.8f, 0.8f, 0.8f);
        whiteMat.specular = Engine::vec3(0.3f, 0.3f, 0.3f);
        whiteMat.shininess = 8.0f;

        auto planeMesh = Engine::MeshFactory::createPlane(30.0f, 30.0f, 15, 15);
        auto ground = m_scene.createObject(planeMesh, whiteMat);
        ground->transform.position = Engine::vec3(0.0f, 0.0f, 0.0f);

        float row1Z = -3.0f;
        float spacing = 3.5f;
        float startX = -7.0f;

        {
            auto mesh = Engine::MeshFactory::createCube(1.2f);
            auto obj = m_scene.createObject(mesh, redMat);
            obj->transform.position = Engine::vec3(startX + spacing * 0, 1.0f, row1Z);
            m_animatedObjects.push_back({ obj, AnimType::Rotate });
        }

        {
            auto mesh = Engine::MeshFactory::createSphere(0.7f, 32, 16);
            auto obj = m_scene.createObject(mesh, orangeMat);
            obj->transform.position = Engine::vec3(startX + spacing * 1, 1.0f, row1Z);
            m_animatedObjects.push_back({ obj, AnimType::Bob });
        }

        {
            auto mesh = Engine::MeshFactory::createCylinder(0.5f, 1.5f, 32);
            auto obj = m_scene.createObject(mesh, yellowMat);
            obj->transform.position = Engine::vec3(startX + spacing * 2, 0.75f, row1Z);
            m_animatedObjects.push_back({ obj, AnimType::RotateAndBob });
        }

        {
            auto mesh = Engine::MeshFactory::createPlane(2.0f, 2.0f, 5, 5);
            auto obj = m_scene.createObject(mesh, greenMat);
            obj->transform.position = Engine::vec3(startX + spacing * 3, 1.0f, row1Z);
            obj->transform.rotation.x = 90.0f;
            m_animatedObjects.push_back({ obj, AnimType::Spin });
        }

        {
            auto mesh = Engine::MeshFactory::createQuad(1.5f, 1.5f);
            auto obj = m_scene.createObject(mesh, cyanMat);
            obj->transform.position = Engine::vec3(startX + spacing * 4, 1.0f, row1Z);
            m_animatedObjects.push_back({ obj, AnimType::Spin });
        }

        float row2Z = 3.0f;

        {
            auto mesh = Engine::MeshFactory::createCone(0.6f, 1.5f, 32);
            auto obj = m_scene.createObject(mesh, blueMat);
            obj->transform.position = Engine::vec3(startX + spacing * 0, 0.75f, row2Z);
            m_animatedObjects.push_back({ obj, AnimType::Rotate });
        }

        {
            auto mesh = Engine::MeshFactory::createPyramid(1.2f, 1.5f);
            auto obj = m_scene.createObject(mesh, purpleMat);
            obj->transform.position = Engine::vec3(startX + spacing * 1, 0.75f, row2Z);
            m_animatedObjects.push_back({ obj, AnimType::Rotate });
        }

        {
            auto mesh = Engine::MeshFactory::createCapsule(0.5f, 2.0f, 16, 8);
            auto obj = m_scene.createObject(mesh, magentaMat);
            obj->transform.position = Engine::vec3(startX + spacing * 2, 1.0f, row2Z);
            m_animatedObjects.push_back({ obj, AnimType::Bob });
        }

        {
            auto mesh = Engine::MeshFactory::createTorus(0.8f, 0.25f, 32, 16);
            auto obj = m_scene.createObject(mesh, pinkMat);
            obj->transform.position = Engine::vec3(startX + spacing * 3, 1.0f, row2Z);
            obj->transform.rotation.x = 60.0f;
            m_animatedObjects.push_back({ obj, AnimType::Spin });
        }

        {
            auto mesh = Engine::MeshFactory::createSkyboxCube(1.5f);
            auto obj = m_scene.createObject(mesh, whiteMat);
            obj->transform.position = Engine::vec3(startX + spacing * 4, 1.0f, row2Z);
            m_animatedObjects.push_back({ obj, AnimType::RotateAndBob });
        }

        Engine::Light mainLight(Engine::vec3(8.0f, 12.0f, 8.0f), Engine::vec3(1.0f, 1.0f, 1.0f));
        m_scene.addLight(mainLight);

        Engine::Light fillLight(Engine::vec3(-8.0f, 8.0f, 8.0f), Engine::vec3(0.5f, 0.5f, 0.7f));
        m_scene.addLight(fillLight);

        Engine::Light backLight(Engine::vec3(0.0f, 6.0f, -10.0f), Engine::vec3(0.7f, 0.5f, 0.5f));
        m_scene.addLight(backLight);

        LOG_INFO("Creating skybox...");

        // Skybox Textures
        
        std::array<std::string, 6> skyboxFaces = {
            "assets/textures/skybox/left.png",
            "assets/textures/skybox/right.png",
            "assets/textures/skybox/top.png",
            "assets/textures/skybox/bottom.png",
            "assets/textures/skybox/front.png",
            "assets/textures/skybox/back.png"
        };
        

        m_skybox = std::make_unique<Engine::Skybox>(skyboxFaces, getRenderDevice());

        LOG_INFO("Skybox created successfully");

        LOG_INFO("");
        LOG_INFO("Scene created: {} objects, {} lights",
            m_scene.getObjectCount(), m_scene.getLightCount());
        LOG_INFO("");
        LOG_INFO("=== PRIMITIVE LABELS ===");
        LOG_INFO("  Row 1 (front): Cube, Sphere, Cylinder, Plane, Quad");
        LOG_INFO("  Row 2 (back):  Cone, Pyramid, Capsule, Torus, Skybox Cube");
        LOG_INFO("");
        LOG_INFO("=== CONTROLS ===");
        LOG_INFO("  C: Switch camera mode (Orbit / FPS)");
        LOG_INFO("");
        LOG_INFO("  ORBIT MODE (default):");
        LOG_INFO("    Arrow Keys: Rotate around scene");
        LOG_INFO("    Up/Down Arrows: Raise/lower camera");
        LOG_INFO("    +/- Keys: Zoom in/out");
        LOG_INFO("    R: Reset camera");
        LOG_INFO("");
        LOG_INFO("  FPS MODE:");
        LOG_INFO("    WASD: Move | SPACE/LCTRL: Up/Down");
        LOG_INFO("    Mouse: Look | Shift: Sprint");
        LOG_INFO("    TAB: Toggle mouse lock | R: Reset camera");
        LOG_INFO("");
        LOG_INFO("  VISUALS:");
        LOG_INFO("    P: Toggle wireframe");
        LOG_INFO("    G: Toggle debug grid");
        LOG_INFO("    B: Toggle bounding spheres");
        LOG_INFO("    V: Toggle AABBs");
        LOG_INFO("    O: Toggle OBBs (Oriented Bounding Boxes)");
        LOG_INFO("    F: Toggle camera frustum");
        LOG_INFO("    H: Toggle other camera frustum (visible from outside)");
        LOG_INFO("    L: Toggle light positions");
        LOG_INFO("    K: Toggle skybox");
        LOG_INFO("    1/2: Animation speed");
        LOG_INFO("");
        LOG_INFO("  ESC: Exit");
    }

    void onUpdate(float deltaTime) override
    {
        Engine::ShaderManager::get().update();

        if (Engine::Input::isKeyJustPressed(SDLK_P, true))
        {
            m_wireframeMode = !m_wireframeMode;
            getRenderer()->setPolygonMode(m_wireframeMode ? Engine::PolygonMode::Line : Engine::PolygonMode::Fill);
            LOG_INFO("Wireframe: {}", m_wireframeMode ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_G, true))
        {
            m_showDebugGrid = !m_showDebugGrid;
            LOG_INFO("Debug grid: {}", m_showDebugGrid ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_K, true))
        {
            m_showSkybox = !m_showSkybox;
            LOG_INFO("Skybox: {}", m_showSkybox ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_B, true))
        {
            m_showBoundingSpheres = !m_showBoundingSpheres;
            LOG_INFO("Bounding spheres: {}", m_showBoundingSpheres ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_V, true))
        {
            m_showAABBs = !m_showAABBs;
            LOG_INFO("AABBs: {}", m_showAABBs ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_O, true))
        {
            m_showOBBs = !m_showOBBs;
            LOG_INFO("OBBs (Oriented Bounding Boxes): {}", m_showOBBs ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_F, true))
        {
            m_showFrustum = !m_showFrustum;
            LOG_INFO("Camera Frustum: {}", m_showFrustum ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_1, true))
        {
            m_animSpeed = std::max(0.0f, m_animSpeed - 0.5f);
            LOG_INFO("Animation speed: {:.1f}x", m_animSpeed);
        }

        if (Engine::Input::isKeyJustPressed(SDLK_2, true))
        {
            m_animSpeed = std::min(5.0f, m_animSpeed + 0.5f);
            LOG_INFO("Animation speed: {:.1f}x", m_animSpeed);
        }

        if (Engine::Input::isKeyJustPressed(SDLK_C, true))
        {
            m_cameraMode = (m_cameraMode == CameraMode::FPS) ? CameraMode::Orbit : CameraMode::FPS;

            if (m_cameraMode == CameraMode::FPS)
            {
                LOG_INFO("Camera Mode: FPS (WASD + Mouse)");
                m_mouseLocked = true;
                Engine::Input::setMouseCursorLocked(true);
                Engine::Input::setMouseCursorVisible(false);
            }
            else
            {
                LOG_INFO("Camera Mode: ORBIT (Arrow Keys, +/- to zoom)");
                m_mouseLocked = false;
                Engine::Input::setMouseCursorLocked(false);
                Engine::Input::setMouseCursorVisible(true);
            }
        }

        if (Engine::Input::isKeyJustPressed(SDLK_H, true))
        {
            m_showOtherCameraFrustum = !m_showOtherCameraFrustum;
            LOG_INFO("Other Camera Frustum: {}", m_showOtherCameraFrustum ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_L, true))
        {
            m_showLights = !m_showLights;
            LOG_INFO("Light positions: {}", m_showLights ? "ON" : "OFF");
        }

        if (m_cameraMode == CameraMode::FPS)
        {
            m_fpsCamera->processKeyboard(deltaTime);

            if (m_mouseLocked)
            {
                Engine::vec2 mouseDelta = Engine::Input::getMouseDelta();
                if (mouseDelta.x != 0 || mouseDelta.y != 0)
                {
                    m_fpsCamera->processMouseMovement(mouseDelta.x, mouseDelta.y);
                }
            }

            if (Engine::Input::isKeyJustPressed(SDLK_TAB, true))
            {
                m_mouseLocked = !m_mouseLocked;
                Engine::Input::setMouseCursorLocked(m_mouseLocked);
                Engine::Input::setMouseCursorVisible(!m_mouseLocked);
            }

            if (Engine::Input::isKeyJustPressed(SDLK_R, true))
            {
                m_fpsCamera->reset(Engine::vec3(0.0f, 4.0f, 15.0f), -90.0f, -15.0f);
                LOG_INFO("FPS camera reset");
            }
        }
        else if (m_cameraMode == CameraMode::Orbit)
        {
            float orbitSpeed = 60.0f * deltaTime;
            float zoomSpeed = 8.0f * deltaTime;

            if (Engine::Input::isKeyPressed(SDLK_LEFT))
            {
                m_orbitAngle -= orbitSpeed;
            }
            if (Engine::Input::isKeyPressed(SDLK_RIGHT))
            {
                m_orbitAngle += orbitSpeed;
            }

            if (Engine::Input::isKeyPressed(SDLK_EQUALS) || Engine::Input::isKeyPressed(SDLK_KP_PLUS))
            {
                m_orbitDistance = std::max(8.0f, m_orbitDistance - zoomSpeed);
            }
            if (Engine::Input::isKeyPressed(SDLK_MINUS) || Engine::Input::isKeyPressed(SDLK_KP_MINUS))
            {
                m_orbitDistance = std::min(40.0f, m_orbitDistance + zoomSpeed);
            }

            if (Engine::Input::isKeyPressed(SDLK_UP))
            {
                m_orbitHeight = std::min(25.0f, m_orbitHeight + zoomSpeed);
            }
            if (Engine::Input::isKeyPressed(SDLK_DOWN))
            {
                m_orbitHeight = std::max(2.0f, m_orbitHeight - zoomSpeed);
            }

            Engine::vec3 newPos(
                Engine::cos(Engine::radians(m_orbitAngle)) * m_orbitDistance,
                m_orbitHeight,
                Engine::sin(Engine::radians(m_orbitAngle)) * m_orbitDistance
            );
            m_orbitCamera->setPosition(newPos);

            if (Engine::Input::isKeyJustPressed(SDLK_R, true))
            {
                m_orbitAngle = 0.0f;
                m_orbitDistance = 18.0f;
                m_orbitHeight = 8.0f;
                Engine::vec3 resetPos(m_orbitDistance, m_orbitHeight, 0.0f);
                m_orbitCamera->setPosition(resetPos);
                m_orbitCamera->setTarget(Engine::vec3(0, 2, 0));
                LOG_INFO("Orbit camera reset");
            }
        }

        float time = Engine::EngineTime::getTime() * m_animSpeed;

        for (auto& [obj, type] : m_animatedObjects)
        {
            switch (type)
            {
            case AnimType::Rotate:
                obj->transform.rotation.y = time * 45.0f;
                break;

            case AnimType::Bob:
            {
                
                if (m_bobBaseYs.find(obj) == m_bobBaseYs.end())
                {
                    m_bobBaseYs[obj] = obj->transform.position.y;
                }
                obj->transform.position.y = m_bobBaseYs[obj] + Engine::sin(time * 2.0f) * 0.3f;
                break;
            }

            case AnimType::RotateAndBob:
            {
                obj->transform.rotation.y = time * 45.0f;
                if (m_rotateBobBaseYs.find(obj) == m_rotateBobBaseYs.end())
                {
                    m_rotateBobBaseYs[obj] = obj->transform.position.y;
                }
                obj->transform.position.y = m_rotateBobBaseYs[obj] + Engine::sin(time * 2.0f) * 0.2f;
                break;
            }

            case AnimType::Spin:
                obj->transform.rotation.y = time * 90.0f;
                break;
            }
        }

        if (m_showDebugGrid)
        {
            Engine::GLDebugRenderer::staticDrawGrid(30.0f, 30, Engine::vec3(0.5f, 0.5f, 0.5f));
        }

        if (m_showBoundingSpheres)
        {
            for (auto& obj : m_scene.getObjects())
            {
                auto sphere = obj->getWorldBoundingSphere();
                Engine::GLDebugRenderer::staticDrawSphere(sphere.center, sphere.radius, Engine::vec3(0, 1, 1));
            }
        }

        if (m_showAABBs)
        {
            for (auto& obj : m_scene.getObjects())
            {
                auto aabb = obj->getWorldAABB();
                Engine::GLDebugRenderer::staticDrawBox(aabb.getCenter(), aabb.getExtents(), Engine::vec3(1, 1, 0));
            }
        }

        if (m_showOBBs)
        {
            for (auto& obj : m_scene.getObjects())
            {
                auto aabb = obj->getMesh()->aabb;
                Engine::vec3 modelCenter = (aabb.min + aabb.max) * 0.5f;
                Engine::vec3 halfExtents = (aabb.max - aabb.min) * 0.5f;
                Engine::mat4 rotationMatrix = obj->transform.getRotationMatrix();
                Engine::vec3 scaledCenter = modelCenter * obj->transform.scale;
                Engine::vec3 rotatedCenter = Engine::vec3(rotationMatrix * Engine::vec4(scaledCenter, 0.0f));
                Engine::vec3 worldCenter = obj->transform.position + rotatedCenter;

                Engine::GLDebugRenderer::staticDrawOBB(
                    worldCenter,
                    halfExtents * obj->transform.scale,
                    rotationMatrix,
                    Engine::vec3(1.0f, 0.5f, 1.0f)
                );
            }
        }

        if (m_showFrustum)
        {
            auto& activeCamera = (m_cameraMode == CameraMode::FPS)
                ? static_cast<Engine::CameraBase&>(*m_fpsCamera)
                : static_cast<Engine::CameraBase&>(*m_orbitCamera);

            Engine::mat4 viewProj = activeCamera.getViewProjectionMatrix(getWindow()->getAspectRatio());
            Engine::GLDebugRenderer::staticDrawFrustum(viewProj, Engine::vec3(1, 1, 0));
        }

        if (m_showOtherCameraFrustum)
        {
            if (m_cameraMode == CameraMode::FPS)
            {
                Engine::mat4 orbitVP = m_orbitCamera->getViewProjectionMatrix(getWindow()->getAspectRatio());
                Engine::GLDebugRenderer::staticDrawFrustum(orbitVP, Engine::vec3(0, 1, 1));
            }
            else
            {
                Engine::mat4 fpsVP = m_fpsCamera->getViewProjectionMatrix(getWindow()->getAspectRatio());
                Engine::GLDebugRenderer::staticDrawFrustum(fpsVP, Engine::vec3(1, 1, 0));
            }
        }

        // Light visualization (separate from grid)
        if (m_showLights)
        {
            // Draw all lights with different colors
            const Engine::vec3 lightDebugColors[] = {
                Engine::vec3(1.0f, 1.0f, 0.0f),  // Yellow
                Engine::vec3(1.0f, 0.5f, 0.0f),  // Orange
                Engine::vec3(1.0f, 0.0f, 0.5f),  // Pink
                Engine::vec3(0.5f, 0.0f, 1.0f),  // Purple
                Engine::vec3(0.0f, 1.0f, 1.0f),  // Cyan
                Engine::vec3(0.0f, 1.0f, 0.5f),  // Teal
            };

            int lightIndex = 0;
            for (auto& light : m_scene.getLights())
            {
                // Choose debug color (cycle through colors)
                Engine::vec3 debugColor = lightDebugColors[lightIndex % 6];

                // Draw sphere at light position
                Engine::GLDebugRenderer::staticDrawSphere(
                    light.position,
                    0.4f,
                    debugColor
                );

                // Draw lines from light to scene center (visualize direction)
                Engine::GLDebugRenderer::staticDrawLine(
                    light.position,
                    Engine::vec3(0, 2, 0),  // Scene center
                    debugColor * 0.5f  // Dimmer line
                );

                lightIndex++;
            }
        }
    }

    void onEvent(Engine::Event& event) override {}

    void onRender() override
    {
        auto shader = Engine::ShaderManager::get().getShader("basic");
        if (!shader) return;
        
        if (m_cameraMode == CameraMode::FPS)
        {
            // 1. SKYBOX FIRST (background)
            if (m_showSkybox && m_skybox)
            {
                getRenderer()->beginSkyboxPass();
                m_skybox->render(*m_fpsCamera, *getWindow());
                getRenderer()->endPass();
            }

            // 2. SCENE (foreground, occludes skybox)
            m_scene.render(*m_fpsCamera, *shader, *getWindow(), *getRenderer());

            // 3. DEBUG (wireframes, last)
            Engine::GLDebugRenderer::staticRender(*m_fpsCamera, *getWindow(), *getRenderer());
        }
        else
        {
            // Same for orbit camera
            if (m_showSkybox && m_skybox)
            {
                getRenderer()->beginSkyboxPass();
                m_skybox->render(*m_orbitCamera, *getWindow());
                getRenderer()->endPass();
            }

            m_scene.render(*m_orbitCamera, *shader, *getWindow(), *getRenderer());
            Engine::GLDebugRenderer::staticRender(*m_orbitCamera, *getWindow(), *getRenderer());
        }

        Engine::GLDebugRenderer::staticClear();
        
        logSoakTestStats();
    }

private:
    enum class AnimType { Rotate, Bob, RotateAndBob, Spin };
    enum class CameraMode { FPS, Orbit };

    struct AnimatedObject
    {
        Engine::SceneObject* obj;
        AnimType type;
    };

    struct LabeledPrimitive
    {
        Engine::SceneObject* obj;
        std::string name;
    };

    std::shared_ptr<Engine::IShader> m_shader;
    Engine::Scene m_scene;
    std::optional<Engine::FPSCamera> m_fpsCamera;
    std::optional<Engine::Camera> m_orbitCamera;

    std::vector<AnimatedObject> m_animatedObjects;

    std::unique_ptr<Engine::Skybox> m_skybox;

    float m_animSpeed = 1.0f;
    bool m_mouseLocked = false;
    bool m_wireframeMode = false;
    bool m_showDebugGrid = false;
    bool m_showBoundingSpheres = false;
    bool m_showAABBs = false;
    bool m_showOBBs = false;
    bool m_showFrustum = false;
    bool m_showOtherCameraFrustum = false;
    bool m_showLights = false;
    bool m_showSkybox = true;

    CameraMode m_cameraMode = CameraMode::Orbit;

    float m_orbitAngle = 0.0f;
    float m_orbitDistance = 18.0f;
    float m_orbitHeight = 8.0f;

    std::unordered_map<Engine::SceneObject*, float> m_bobBaseYs;
    std::unordered_map<Engine::SceneObject*, float> m_rotateBobBaseYs;

    static Engine::Application::Config createConfig()
    {
        Engine::Application::Config config;

        // Application settings
        config.app.mode = Mode::Direct;
        config.app.logLevel = Engine::LogLevel::Trace;

        // Window settings
        config.window.title = "Complete Primitive Showcase - All 10 Shapes";
        config.window.width = 1920;
        config.window.height = 1080;
        config.window.vsync = true;      // Keep vsync default
        config.window.fullscreen = true;
        config.window.resizable = true;  // Keep resizable default

        // Render settings
        config.render.wireframeMode = false;
        config.render.showDebugRenderer = true;  // Keep default
        config.render.enableFaceCulling = true;
        config.render.enableDepthTest = true;
        config.render.anisotropicFiltering = 16;
        config.render.msaaSamples = 4;  // Keep default

        return config;
    }
};

Engine::Application* Engine::createApplication()
{
    return new CompletePrimitiveShowcaseApp();
}

int main(int argc, char* argv[])
{
    auto app = Engine::createApplication();
    app->run();
    delete app;
    return 0;
}
