#include "core/Application.h"
#include "core/EngineTime.h"
#include "core/Logger.h"
#include "core/Error.h"
#include "input/Input.h"
#include "renderer/opengl/GLDebugRenderer.h"
#include "renderer/opengl/GLRenderDevice.h"
#include "renderer/AssetManager.h"
#include "renderer/MeshFactory.h"
#include "renderer/ShaderManager.h"
#include <SDL3/SDL.h>

namespace Engine
{
    namespace
    {
        int clampMSAASamples(int requested)
        {
            if (requested == 0) return 0;
            if (requested <= 2) return 2;
            if (requested <= 4) return 4;
            if (requested <= 8) return 8;
            return 16;
        }
    }

    Application* Application::s_instance = nullptr;

    Application::Application(const Config& config)
        : m_config(config), m_mode(config.app.mode)
    {
        s_instance = this;
        initializeCore();
    }

    Application::~Application()
    {
        shutdownCore();
        s_instance = nullptr;
    }

    void Application::initializeCore()
    {
        // Initialize Logger
        Logger::init(m_config.app.logFile);
        Logger::setLevel(m_config.app.logLevel);

        LOG_INFO("=== Engine Initialization Started ===");

        // Validate MSAA samples (same pattern as anisotropic filtering)
        int requestedMSAA = m_config.render.msaaSamples;
        m_config.render.msaaSamples = clampMSAASamples(requestedMSAA);
        if (requestedMSAA != m_config.render.msaaSamples)
        {
            LOG_WARN("MSAA samples {} clamped to {}", requestedMSAA, m_config.render.msaaSamples);
        }

        // Initialize Time system
        EngineTime::init();

        // Initialize Input system
        Input::init();

        // Setup MSAA before window creation
        if (m_config.render.msaaSamples > 0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, m_config.render.msaaSamples);
            LOG_INFO("MSAA: {}x", m_config.render.msaaSamples);
        }
        else
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
            LOG_INFO("MSAA: disabled");
        }

        // === CREATE WINDOW FIRST (establishes OpenGL context) ===
        LOG_INFO("=== Creating Window ===");
        Window::Properties windowProps;
        windowProps.title = m_config.window.title;
        windowProps.width = m_config.window.width;
        windowProps.height = m_config.window.height;
        windowProps.vsync = m_config.window.vsync;
        windowProps.fullscreen = m_config.window.fullscreen;
        windowProps.resizable = m_config.window.resizable;

        m_window = std::make_unique<Window>(windowProps);

        // Check if the window initialized correctly
        if (!m_window->isValid())
        {
            LOG_FATAL("Window initialization failed - cannot continue");
            m_running = false;
            return;
        }

        LOG_INFO("Window created successfully");

        // === NOW CREATE RENDER DEVICE (OpenGL context is active) ===
        LOG_INFO("=== Initializing Render Device ===");
        m_renderDevice = std::make_unique<GLRenderDevice>();

        if (!m_renderDevice)
        {
            LOG_FATAL("Failed to create render device!");
            m_running = false;
            return;
        }

        // Query and log device information (safe now that context exists)
        const char* deviceName = m_renderDevice->getDeviceName();
        const char* rendererName = m_renderDevice->getRendererName();

        LOG_INFO("Render Device: {}", deviceName);
        LOG_INFO("GPU: {}", rendererName);

        // Validate we got valid strings
        ENGINE_ASSERT(deviceName != nullptr, "Render device name is null");
        ENGINE_ASSERT(rendererName != nullptr, "Renderer name is null");

        // === INITIALIZE ASSET SYSTEMS ===
        LOG_INFO("=== Initializing Asset Systems ===");

        // Configure AssetManager with rendering settings
        AssetManager::get().setAnisotropicFiltering(m_config.render.anisotropicFiltering);
        LOG_INFO("Anisotropic filtering: {}x", m_config.render.anisotropicFiltering);

        // Initialize managers with render device (ONCE - guards prevent double-init)
        AssetManager::get().initialize(m_renderDevice.get());
        MeshFactory::initialize(m_renderDevice.get());
        ShaderManager::get().initialize(m_renderDevice.get());

        // Initialize ImGui after window
        m_imguiLayer.init(m_window.get());

        // Initialize Debug Renderer (static call on concrete class)
        GLDebugRenderer::staticInit();

        // Register ImGui's event processor with Window
        m_window->setRawEventCallback([this](const SDL_Event& sdl_event)
            {
                m_imguiLayer.processEvent(sdl_event);
            });

        // Set callbacks if window is valid
        m_window->setEventCallback([this](Event& e)
            {
                Input::onEvent(e);

                // To close the app
                if (e.type == EventType::WindowClose)
                {
                    m_running = false;
                }
                if (e.type == EventType::KeyPressed && e.keyboard.key == SDLK_ESCAPE)
                {
                    m_running = false;
                }

                // F1 toggle for ImGui Debug UI
                if (e.type == EventType::KeyPressed && e.keyboard.key == SDLK_F1)
                {
                    m_imguiLayer.toggleDebugUI();
                }

                // Pass to user
                onEvent(e);
            });

        // OpenGL settings
        m_renderer = m_renderDevice->createRenderer();

        // Set intial viewport through renderer
        m_renderer->setViewport(0, 0, m_window->getWidth(), m_window->getHeight());

        // Replace OpenGL calls:
        m_renderer->setDepthTest(true);
        m_renderer->setFrontFace(FrontFace::CCW);

        if (m_config.render.msaaSamples > 0)
        {
            m_renderer->setMSAA(true);
            m_renderer->setAlphaToCoverage(true);
        }

        if (m_config.render.wireframeMode)
        {
            m_renderer->setPolygonMode(PolygonMode::Line);
        }

        if (m_config.render.enableFaceCulling)
        {
            m_renderer->setFaceCulling(true, CullMode::Back);
        }

        LOG_INFO("Render config: wireframe={}, culling={}",
            m_config.render.wireframeMode,
            m_config.render.enableFaceCulling);

        // Log what mode we're in
        const char* modeName = m_mode == Mode::Direct ? "Direct"
            : m_mode == Mode::World ? "World"
            : "Editor";
        LOG_INFO("Application initialized in {} mode", modeName);

        LOG_INFO("=== Application Initialization Complete ===");
    }

    void Application::run()
    {
        // Check if initialization failed
        if (!m_running)
        {
            LOG_ERROR("Application not properly initialized, aborting run");
            return;
        }

        onInitialize();

        while (m_running)
        {
            EngineTime::update();
            EngineTime::updateFixed();
            Input::beginFrame();
            m_imguiLayer.beginFrame();
            m_window->pollEvents();

            // Fixed timestep loop for physics 60hz
            while (EngineTime::shouldDoFixedUpdate())
            {
                onFixedUpdate(EngineTime::getFixedDeltaTime());
                EngineTime::consumeFixedUpdate();
            }

            // Variable timestep for gameplay/animations
            onUpdate(EngineTime::getDeltaTime());

            Input::endFrame();

            // Clear with different colors per mode (visual debugging!)
            switch (m_mode) {
            case Mode::Direct:
                m_renderer->clearColor(0.39f, 0.58f, 0.93f, 1.0f);
                break;
            case Mode::World:
                m_renderer->clearColor(0.2f, 0.3f, 0.3f, 1.0f);
                break;
            case Mode::Editor:
                m_renderer->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
                break;
            }
            m_renderer->clearScreen(true, true, false);  // color, depth, stencil

            onRender();

            /* commented out for soak test
            static int frameCount = 0;
            if (++frameCount % 300 == 0)  // Every 5 seconds at 60fps
            {
                int changes = m_renderer->getStateChanges();
                int saved = m_renderer->getStateChangesSaved();
                int total = changes + saved;

                if (total > 0)
                {
                    float reductionPercent = (saved / (float)total) * 100.0f;
                    LOG_INFO("Renderer Stats: {} state changes, {} saved ({:.1f}% reduction)",
                        changes, saved, reductionPercent);
                }

                m_renderer->resetStats();
            }
            */

            // Render ImGui
            m_imguiLayer.render();

            m_window->swapBuffers();
        }

        onShutdown();

        // Shutdown ImGui before window destruction
        m_imguiLayer.shutdown();
    }

    void Application::shutdownCore()
    {
        GLDebugRenderer::staticShutdown();
        Input::shutdown();
        m_renderDevice.reset();  // Destroy render device before window
        m_window.reset();
        Logger::shutdown();
    }

    int Application::getAnisotropicFiltering() const
    {
        return m_config.render.anisotropicFiltering;
    }
}