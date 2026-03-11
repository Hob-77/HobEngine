#include "math/EngineMath.h"
#include "renderer/camera/FPSCamera.h"
#include "renderer/MeshFactory.h"
#include "renderer/opengl/GLDebugRenderer.h"
#include "renderer/ShaderManager.h"
#include "renderer/Skybox.h"
#include "core/Application.h"
#include "core/EngineTime.h"
#include "input/Input.h"
#include "scene/Scene.h"
#include "scene/SceneObject.h"
#include <optional>

class ComprehensiveShowcaseApp : public Engine::Application
{
public:
    ComprehensiveShowcaseApp()
        : Application(createConfig())
    {
    }

    void onInitialize() override
    {
        LOG_INFO("=== HOBENGINE - COMPREHENSIVE SHOWCASE ===");
        LOG_INFO("Demonstrating ALL 10 MeshFactory primitives:");
        LOG_INFO("  Row 1: Cube, Sphere, Cylinder, Plane, Quad");
        LOG_INFO("  Row 2: Cone, Pyramid, Capsule, Torus, Skybox Cube");
        LOG_INFO("");

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
        settings.walkSpeed       = 8.0f;
        settings.sprintMultiplier = 2.5f;
        settings.flyMode         = true;

        Engine::Input::setMouseCursorLocked(true);
        Engine::Input::setMouseCursorVisible(false);

        // --- Materials ---

        Engine::Material redMat, orangeMat, yellowMat, greenMat, cyanMat;

        redMat.diffuse    = Engine::vec3(1.0f, 0.2f, 0.2f);
        redMat.specular   = Engine::vec3(1.0f, 1.0f, 1.0f);
        redMat.shininess  = 32.0f;

        orangeMat.diffuse   = Engine::vec3(1.0f, 0.6f, 0.2f);
        orangeMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        orangeMat.shininess = 64.0f;

        yellowMat.diffuse   = Engine::vec3(1.0f, 1.0f, 0.2f);
        yellowMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        yellowMat.shininess = 32.0f;

        greenMat.diffuse   = Engine::vec3(0.2f, 1.0f, 0.2f);
        greenMat.specular  = Engine::vec3(0.5f, 0.5f, 0.5f);
        greenMat.shininess = 8.0f;

        cyanMat.diffuse   = Engine::vec3(0.2f, 1.0f, 1.0f);
        cyanMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        cyanMat.shininess = 32.0f;

        Engine::Material blueMat, purpleMat, magentaMat, pinkMat, whiteMat;

        blueMat.diffuse   = Engine::vec3(0.2f, 0.4f, 1.0f);
        blueMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        blueMat.shininess = 32.0f;

        purpleMat.diffuse   = Engine::vec3(0.6f, 0.2f, 1.0f);
        purpleMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        purpleMat.shininess = 32.0f;

        magentaMat.diffuse   = Engine::vec3(1.0f, 0.2f, 1.0f);
        magentaMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        magentaMat.shininess = 32.0f;

        pinkMat.diffuse   = Engine::vec3(1.0f, 0.4f, 0.7f);
        pinkMat.specular  = Engine::vec3(1.0f, 1.0f, 1.0f);
        pinkMat.shininess = 64.0f;

        whiteMat.diffuse   = Engine::vec3(0.8f, 0.8f, 0.8f);
        whiteMat.specular  = Engine::vec3(0.3f, 0.3f, 0.3f);
        whiteMat.shininess = 8.0f;

        // --- Ground ---

        auto planeMesh = Engine::MeshFactory::createPlane(30.0f, 30.0f, 15, 15);
        auto ground    = m_scene.createObject(planeMesh, whiteMat);
        ground->transform.position = Engine::vec3(0.0f, 0.0f, 0.0f);

        // --- Row 1: Cube, Sphere, Cylinder, Plane, Quad ---

        constexpr float row1Z   = -3.0f;
        constexpr float row2Z   =  3.0f;
        constexpr float spacing =  3.5f;
        constexpr float startX  = -7.0f;

        {
            auto mesh = Engine::MeshFactory::createCube(1.2f);
            auto obj  = m_scene.createObject(mesh, redMat);
            obj->transform.position = Engine::vec3(startX, 1.0f, row1Z);
        }

        {
            auto mesh = Engine::MeshFactory::createSphere(0.7f, 32, 16);
            auto obj  = m_scene.createObject(mesh, orangeMat);
            obj->transform.position = Engine::vec3(startX + spacing * 1, 1.0f, row1Z);
        }

        {
            auto mesh = Engine::MeshFactory::createCylinder(0.5f, 1.5f, 32);
            auto obj  = m_scene.createObject(mesh, yellowMat);
            obj->transform.position = Engine::vec3(startX + spacing * 2, 0.75f, row1Z);
        }

        {
            auto mesh = Engine::MeshFactory::createPlane(2.0f, 2.0f, 5, 5);
            auto obj  = m_scene.createObject(mesh, greenMat);
            obj->transform.position  = Engine::vec3(startX + spacing * 3, 1.0f, row1Z);
            obj->transform.rotation.x = 90.0f;
        }

        {
            auto mesh = Engine::MeshFactory::createQuad(1.5f, 1.5f);
            auto obj  = m_scene.createObject(mesh, cyanMat);
            obj->transform.position = Engine::vec3(startX + spacing * 4, 1.0f, row1Z);
        }

        // --- Row 2: Cone, Pyramid, Capsule, Torus, Skybox Cube ---

        {
            auto mesh = Engine::MeshFactory::createCone(0.6f, 1.5f, 32);
            auto obj  = m_scene.createObject(mesh, blueMat);
            obj->transform.position = Engine::vec3(startX, 0.75f, row2Z);
        }

        {
            auto mesh = Engine::MeshFactory::createPyramid(1.2f, 1.5f);
            auto obj  = m_scene.createObject(mesh, purpleMat);
            obj->transform.position = Engine::vec3(startX + spacing * 1, 0.75f, row2Z);
        }

        {
            auto mesh = Engine::MeshFactory::createCapsule(0.5f, 2.0f, 16, 8);
            auto obj  = m_scene.createObject(mesh, magentaMat);
            obj->transform.position = Engine::vec3(startX + spacing * 2, 1.0f, row2Z);
        }

        {
            auto mesh = Engine::MeshFactory::createTorus(0.8f, 0.25f, 32, 16);
            auto obj  = m_scene.createObject(mesh, pinkMat);
            obj->transform.position  = Engine::vec3(startX + spacing * 3, 1.0f, row2Z);
            obj->transform.rotation.x = 60.0f;
        }

        {
            auto mesh = Engine::MeshFactory::createSkyboxCube(1.5f);
            auto obj  = m_scene.createObject(mesh, whiteMat);
            obj->transform.position = Engine::vec3(startX + spacing * 4, 1.0f, row2Z);
        }

        // --- Lights ---

        m_scene.addLight(Engine::Light(Engine::vec3( 8.0f, 12.0f,   8.0f), Engine::vec3(1.0f, 1.0f, 1.0f)));
        m_scene.addLight(Engine::Light(Engine::vec3(-8.0f,  8.0f,   8.0f), Engine::vec3(0.5f, 0.5f, 0.7f)));
        m_scene.addLight(Engine::Light(Engine::vec3( 0.0f,  6.0f, -10.0f), Engine::vec3(0.7f, 0.5f, 0.5f)));

        // --- Skybox ---

        std::array<std::string, 6> skyboxFaces = {
            "assets/textures/skybox/left.png",
            "assets/textures/skybox/right.png",
            "assets/textures/skybox/top.png",
            "assets/textures/skybox/bottom.png",
            "assets/textures/skybox/front.png",
            "assets/textures/skybox/back.png"
        };

        m_skybox = std::make_unique<Engine::Skybox>(skyboxFaces, getRenderDevice());

        LOG_INFO("Scene: {} objects, {} lights", m_scene.getObjectCount(), m_scene.getLightCount());
        LOG_INFO("");
        LOG_INFO("=== CONTROLS ===");
        LOG_INFO("  WASD: Move  |  Mouse: Look  |  Shift: Sprint  |  Space/LCtrl: Up/Down");
        LOG_INFO("  TAB: Toggle mouse lock  |  R: Reset camera");
        LOG_INFO("");
        LOG_INFO("  P: Wireframe  |  B: Bounding spheres  |  V: AABBs");
        LOG_INFO("  L: Light positions  |  K: Skybox");
        LOG_INFO("");
        LOG_INFO("  ESC: Exit");
    }

    void onUpdate(float deltaTime) override
    {
        Engine::ShaderManager::get().update();

        // --- Camera ---

        m_fpsCamera->processKeyboard(deltaTime);

        if (m_mouseLocked)
        {
            Engine::vec2 mouseDelta = Engine::Input::getMouseDelta();
            if (mouseDelta.x != 0 || mouseDelta.y != 0)
                m_fpsCamera->processMouseMovement(mouseDelta.x, mouseDelta.y);
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
            LOG_INFO("Camera reset");
        }

        // --- Debug Toggles ---

        if (Engine::Input::isKeyJustPressed(SDLK_P, true))
        {
            m_wireframeMode = !m_wireframeMode;
            getRenderer()->setPolygonMode(m_wireframeMode ? Engine::PolygonMode::Line : Engine::PolygonMode::Fill);
            LOG_INFO("Wireframe: {}", m_wireframeMode ? "ON" : "OFF");
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

        if (Engine::Input::isKeyJustPressed(SDLK_L, true))
        {
            m_showLights = !m_showLights;
            LOG_INFO("Light positions: {}", m_showLights ? "ON" : "OFF");
        }

        if (Engine::Input::isKeyJustPressed(SDLK_K, true))
        {
            m_showSkybox = !m_showSkybox;
            LOG_INFO("Skybox: {}", m_showSkybox ? "ON" : "OFF");
        }

        // --- Debug Drawing ---

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

        if (m_showLights)
        {
            const Engine::vec3 lightDebugColors[] = {
                Engine::vec3(1.0f, 1.0f, 0.0f),
                Engine::vec3(1.0f, 0.5f, 0.0f),
                Engine::vec3(1.0f, 0.0f, 0.5f),
            };

            int lightIndex = 0;
            for (auto& light : m_scene.getLights())
            {
                Engine::vec3 debugColor = lightDebugColors[lightIndex % 3];
                Engine::GLDebugRenderer::staticDrawSphere(light.position, 0.4f, debugColor);
                Engine::GLDebugRenderer::staticDrawLine(light.position, Engine::vec3(0, 2, 0), debugColor * 0.5f);
                lightIndex++;
            }
        }
    }

    void onEvent(Engine::Event& event) override {}

    void onRender() override
    {
        auto shader = Engine::ShaderManager::get().getShader("basic");
        if (!shader) return;

        if (m_showSkybox && m_skybox)
        {
            getRenderer()->beginSkyboxPass();
            m_skybox->render(*m_fpsCamera, *getWindow());
            getRenderer()->endPass();
        }

        m_scene.render(*m_fpsCamera, *shader, *getWindow(), *getRenderer());
        Engine::GLDebugRenderer::staticRender(*m_fpsCamera, *getWindow(), *getRenderer());
        Engine::GLDebugRenderer::staticClear();
    }

private:
    std::shared_ptr<Engine::IShader>  m_shader;
    Engine::Scene                     m_scene;
    std::optional<Engine::FPSCamera>  m_fpsCamera;
    std::unique_ptr<Engine::Skybox>   m_skybox;

    bool m_mouseLocked         = true;
    bool m_wireframeMode       = false;
    bool m_showBoundingSpheres = false;
    bool m_showAABBs           = false;
    bool m_showLights          = false;
    bool m_showSkybox          = true;

    static Engine::Application::Config createConfig()
    {
        Engine::Application::Config config;

        config.app.mode     = Mode::Direct;
        config.app.logLevel = Engine::LogLevel::Info;

        config.window.title      = "HobEngine - Comprehensive Showcase";
        config.window.width      = 1280;
        config.window.height     = 720;
        config.window.vsync      = false;
        config.window.fullscreen = false;
        config.window.resizable  = true;

        config.render.wireframeMode        = false;
        config.render.showDebugRenderer    = true;
        config.render.enableFaceCulling    = true;
        config.render.enableDepthTest      = true;
        config.render.anisotropicFiltering = 16;
        config.render.msaaSamples          = 4;

        return config;
    }
};

Engine::Application* Engine::createApplication()
{
    return new ComprehensiveShowcaseApp();
}

int main(int argc, char* argv[])
{
    auto app = Engine::createApplication();
    app->run();
    delete app;
    return 0;
}
