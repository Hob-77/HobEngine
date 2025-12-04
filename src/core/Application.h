#pragma once
#include <memory>
#include <string>
#include "core/Window.h"
#include "events/Event.h"
#include "core/Logger.h"
#include "ui/ImGuiLayer.h"
#include "renderer/interface/IRenderDevice.h"
#include "renderer/interface/IRenderer.h"

/*
 * Application.h
 *
 * PURPOSE:
 * Core application framework managing engine lifecycle, main loop, and subsystem coordination.
 * Base class for all applications (games, tools, editors) built with the engine. Handles
 * initialization order, shutdown sequence, and frame timing. Configuration-driven design
 * with sensible defaults.
 *
 * DESIGN RATIONALE (September 23, 2025, Major Refactors November 7-14, 2025):
 * Problem: Need clean entry point for user applications. Need consistent initialization
 * order (complex dependencies). Need flexible configuration (avoid constructor bloat).
 * Need abstraction (hide engine complexity). Need frame loop (variable + fixed timestep).
 *
 * Solution: Base Application class with configuration structs and virtual hooks.
 * - Config structs: Extensible, readable (designated initializers)
 * - Virtual hooks: User customization (onInitialize, onUpdate, onRender)
 * - Initialization order: Carefully sequenced (dependencies resolved)
 * - Protected accessors: Derived classes access engine systems
 * - November 7: Interface integration (IRenderDevice, GLRenderDevice)
 * - November 14: Removed OpenGL calls (moved to GLRenderer interface)
 * - Result: Clean, maintainable, API-agnostic application framework
 *
 * Key Insight: Application is engine's face to user (must be clean, intuitive). Config
 * structs better than constructor parameters (extensible, self-documenting). Initialization
 * order critical (Logger first, RenderDevice early, Window after MSAA setup). Virtual
 * hooks natural for user customization (familiar pattern). November refactors prepared
 * for Vulkan (interface abstraction, no direct OpenGL calls).
 *
 * DESIGN PHILOSOPHY:
 * - Configuration-driven: Config structs with defaults
 * - Virtual hooks: User customization points
 * - Careful initialization: Dependencies resolved in order
 * - Protected accessors: Derived classes access systems
 * - Interface abstraction: API-agnostic (November 7-14)
 *
 * KEY CONCEPTS:
 * 1. Configuration Structs:
 *    - ApplicationConfig: Mode, logging
 *    - WindowConfig: Title, dimensions, VSync
 *    - RenderConfig: Wireframe, culling, MSAA, AF
 *    - Extensible: Add fields without breaking code
 *
 * 2. Virtual Hooks (Override in Derived):
 *    - onInitialize(): Load assets, setup scene
 *    - onUpdate(dt): Variable timestep (rendering, animations)
 *    - onFixedUpdate(dt): Fixed timestep (physics, 60Hz)
 *    - onRender(): Scene rendering, UI
 *    - onEvent(e): Event handling
 *    - onShutdown(): Cleanup, save data
 *
 * 3. Initialization Order (Critical):
 *    - Logger first (enable logging)
 *    - RenderDevice early (graphics API)
 *    - MSAA before window (OpenGL requirement)
 *    - User onInitialize() last (after engine ready)
 *
 * 4. Interface Abstraction (November 7-14):
 *    - IRenderDevice: Factory for resources
 *    - IRenderer: State management (no OpenGL in Application)
 *    - Prepares for Vulkan (same code, different implementation)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === MINIMAL APPLICATION (ALL DEFAULTS) ===
 * class TestApp : public Application {
 * public:
 *     TestApp() : Application() {}
 *
 *     void onInitialize() override {
 *         LOG_INFO("TestApp initialized");
 *     }
 *
 *     void onUpdate(float dt) override {
 *         // Game logic
 *     }
 *
 *     void onRender() override {
 *         // Rendering
 *     }
 * };
 *
 * // Entry point (user-defined)
 * Engine::Application* Engine::createApplication() {
 *     return new TestApp();
 * }
 *
 * // === CUSTOM CONFIGURATION ===
 * class MyGame : public Application {
 * public:
 *     MyGame() : Application({
 *         .app = {
 *             .mode = Mode::World,           // Full scene graph
 *             .logLevel = LogLevel::Trace    // Verbose logging
 *         },
 *         .window = {
 *             .title = "My Game",
 *             .width = 1920,
 *             .height = 1080,
 *             .vsync = true
 *         },
 *         .render = {
 *             .anisotropicFiltering = 16,    // Max texture quality
 *             .msaaSamples = 4,              // 4x anti-aliasing
 *             .enableFaceCulling = true      // Optimize rendering
 *         }
 *     }) {}
 *
 *     void onInitialize() override {
 *         // Access engine systems
 *         float aspect = getWindow()->getAspectRatio();
 *         auto mesh = getRenderDevice()->createMesh(...);
 *
 *         // Load game assets
 *         loadLevel("level1.map");
 *     }
 *
 *     void onUpdate(float dt) override {
 *         // Variable timestep (smooth animations)
 *         player.update(dt);
 *         camera.update(dt);
 *     }
 *
 *     void onFixedUpdate(float dt) override {
 *         // Fixed timestep (stable physics, 60Hz)
 *         physics.update(dt);
 *         collisions.update(dt);
 *     }
 *
 *     void onRender() override {
 *         // Render scene
 *         scene.render(camera, shader, window, renderer);
 *     }
 *
 *     void onEvent(Event& e) override {
 *         if (e.type == EventType::WindowClose) {
 *             quit();
 *         }
 *     }
 * };
 *
 * // === ACCESSING ENGINE SYSTEMS ===
 * void MyGame::onInitialize() {
 *     // Window queries (protected accessor)
 *     Window* window = getWindow();
 *     int width = window->getWidth();
 *     float aspect = window->getAspectRatio();
 *
 *     // RenderDevice (protected accessor)
 *     IRenderDevice* device = getRenderDevice();
 *     auto shader = device->createShaderFromFiles("vert.glsl", "frag.glsl");
 *     auto texture = device->createTexture("wood.jpg");
 *
 *     // Renderer (protected accessor)
 *     IRenderer* renderer = getRenderer();
 *     renderer->setWireframeMode(true);
 * }
 * ```
 *
 * CONFIGURATION OPTIONS - Details:
 *
 * ApplicationConfig:
 * - mode: Application mode (Direct, World, Editor)
 *   - Direct: Basic rendering (prototyping, simple apps)
 *   - World: Full scene graph with lighting (games, 3D apps)
 *   - Editor: Debug UI and tools (level editors, content creation)
 * - logLevel: Minimum log level (Trace, Info, Warning, Error, Fatal)
 * - logFile: Log file path (default: "engine.log")
 *
 * WindowConfig:
 * - title: Window title bar text
 * - width, height: Window dimensions (pixels)
 * - vsync: Synchronize with monitor refresh (smooth rendering)
 * - fullscreen: Exclusive fullscreen mode
 * - resizable: User can resize window
 *
 * RenderConfig:
 * - wireframeMode: Render as wireframes (debugging)
 * - showDebugRenderer: Enable debug visualization
 * - enableFaceCulling: Cull back-faces (performance optimization)
 * - enableDepthTest: Enable Z-buffer (required for 3D)
 * - anisotropicFiltering: Texture quality at oblique angles
 *   - 0: Off (blurry textures)
 *   - 4-8: Medium quality (good balance)
 *   - 16: Max quality (recommended, nearly free performance cost)
 * - msaaSamples: Multi-sample anti-aliasing quality
 *   - 0: Off (jagged edges)
 *   - 4: Standard (recommended, good quality/performance)
 *   - 8-16: High quality (expensive)
 *
 * VIRTUAL HOOKS - User Customization Points:
 *
 * ```cpp
 * class MyGame : public Application {
 * public:
 *     // Called once after engine initialization
 *     void onInitialize() override {
 *         // Load assets (textures, models, shaders)
 *         // Setup scene (spawn objects, configure camera)
 *         // Initialize game state
 *     }
 *
 *     // Called every frame (variable timestep)
 *     void onUpdate(float deltaTime) override {
 *         // Game logic (player movement, AI, animations)
 *         // Camera updates (smooth following, rotation)
 *         // Particle systems, UI state
 *     }
 *
 *     // Called at 60Hz (fixed timestep)
 *     void onFixedUpdate(float fixedDelta) override {
 *         // Physics simulation (stable, deterministic)
 *         // Collision detection and response
 *         // Network sync (all clients run same physics)
 *     }
 *
 *     // Called every frame (after update)
 *     void onRender() override {
 *         // Scene rendering (3D world)
 *         // UI rendering (HUD, menus)
 *         // Debug visualization (optional)
 *     }
 *
 *     // Called per event
 *     void onEvent(Event& e) override {
 *         // Window events (close, resize, focus)
 *         // Input events (keyboard, mouse)
 *         // Custom events (game-specific)
 *     }
 *
 *     // Called once before shutdown
 *     void onShutdown() override {
 *         // Save game state (progress, settings)
 *         // Free custom resources
 *         // Disconnect from servers
 *     }
 * };
 * ```
 *
 * MAIN LOOP - Frame Flow:
 *
 * ```cpp
 * void Application::run() {
 *     while (m_running) {
 *         // 1. UPDATE TIME
 *         EngineTime::update();
 *
 *         // 2. POLL EVENTS
 *         m_window->pollEvents();
 *         // - SDL events polled
 *         // - Input system updated
 *         // - onEvent() called per event
 *
 *         // 3. FIXED UPDATE (PHYSICS)
 *         EngineTime::updateFixed();
 *         while (EngineTime::shouldDoFixedUpdate()) {
 *             float fixedDelta = EngineTime::getFixedDeltaTime();
 *             onFixedUpdate(fixedDelta);  // 60Hz, deterministic
 *             EngineTime::consumeFixedUpdate();
 *         }
 *
 *         // 4. VARIABLE UPDATE (GAME LOGIC)
 *         float deltaTime = EngineTime::getDeltaTime();
 *         onUpdate(deltaTime);  // Adaptive, smooth
 *
 *         // 5. RENDER
 *         onRender();  // Scene rendering, UI
 *
 *         // 6. PRESENT
 *         m_window->swapBuffers();  // Show frame
 *     }
 * }
 * ```
 *
 * INITIALIZATION ORDER - Critical Dependencies:
 *
 * ```cpp
 * void Application::initializeCore() {
 *     // 1. LOGGER (First, enables all other logging)
 *     Logger::init(m_config.app.logFile);
 *     Logger::setLevel(m_config.app.logLevel);
 *
 *     // 2. RENDER DEVICE (Graphics API initialization)
 *     m_renderDevice = std::make_unique<GLRenderDevice>();  // November 7
 *     // - OpenGL context created
 *     // - GLAD function loading
 *
 *     // 3. DEVICE VALIDATION (Test shader compilation)
 *     if (!m_renderDevice->testShaderCompilation()) {
 *         LOG_FATAL("Shader compilation test failed");
 *         exit(1);
 *     }
 *
 *     // 4. ASSET MANAGER CONFIG (Before loading assets)
 *     AssetManager::get().setAnisotropicFiltering(
 *         m_config.render.anisotropicFiltering);
 *
 *     // 5. SYSTEM INITIALIZATION (Managers need render device)
 *     AssetManager::get().initialize(m_renderDevice.get());
 *     ShaderManager::get().initialize(m_renderDevice.get());
 *     MeshFactory::initialize(m_renderDevice.get());
 *
 *     // 6. MSAA SETUP (Before window creation, OpenGL requirement)
 *     if (m_config.render.msaaSamples > 0) {
 *         SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
 *         SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,
 *                             m_config.render.msaaSamples);
 *     }
 *
 *     // 7. WINDOW CREATION (Graphics context)
 *     m_window = std::make_unique<Window>();
 *
 *     // 8. RENDERER SETUP (OpenGL state, November 14)
 *     m_renderer = std::make_unique<GLRenderer>();  // November 14
 *     m_renderer->setDepthTest(m_config.render.enableDepthTest);
 *     m_renderer->setFaceCulling(m_config.render.enableFaceCulling);
 *     // No direct OpenGL calls in Application anymore!
 *
 *     // 9. IMGUI INITIALIZATION (UI framework)
 *     m_imguiLayer.init(m_window.get());
 *
 *     // 10. TIME SYSTEM
 *     EngineTime::init();
 *
 *     // 11. USER INITIALIZATION (Last, after engine ready)
 *     onInitialize();
 * }
 *```
 *
 * Why this order ?
 * - Logger first : All other systems log during init
 * - RenderDevice early : Managers need it for resource creation
 * - MSAA before window : OpenGL requires attributes set before context creation
 * - Renderer after window : Needs OpenGL context to set state
 * - User init last : Engine fully ready for game code
 *
 * SHUTDOWN ORDER - Reverse of Initialization :
 *
 *```cpp
 * void Application::shutdownCore() {
 *     // 1. USER SHUTDOWN (First, clean up game state)
 *onShutdown();
     *
         *     // 2. IMGUI CLEANUP (UI resources)
         *m_imguiLayer.shutdown();
     *
         *     // 3. WINDOW DESTRUCTION (GL context destroyed automatically)
         *m_window.reset();  // RAII cleanup
     *
         *     // 4. RENDER DEVICE (Last, after all resources freed)
         *m_renderDevice.reset();
     *
         *     // 5. LOGGER (Last, flush all logs)
         *Logger::shutdown();
     *
 }
 *```
     *
     * PROTECTED ACCESSORS - Derived Class Access :
 *
     *```cpp
     * class MyGame : public Application {
     *void onInitialize() override {
         *         // Access window (Window*)
             *Window* window = getWindow();
         *float aspect = window->getAspectRatio();
         *
             *         // Access render device (IRenderDevice*)
             *IRenderDevice* device = getRenderDevice();
         *auto shader = device->createShaderFromFiles(...);
         *
             *         // Access renderer (IRenderer*)
             *IRenderer* renderer = getRenderer();
         *renderer->setWireframeMode(true);
         *
     }
     *
 };
 *```
  *
 * Why pointers? (Not references)
 * - Protected members return pointers (Window*, IRenderDevice*, IRenderer*)
 * - User must dereference for reference: getWindow()->getWidth()
 * - Allows null checks (though should never be null after init)
 *
 * INHERITANCE PATTERN - Access Control:
 *
 * ```
 * Application (base class)
 * |
 * +-- private: m_window, m_renderDevice, m_running
 * |   (Only Application can access)
 * |
 * +-- protected: getWindow(), getRenderDevice(), getRenderer()
 * |   (Application + derived classes can access)
 * |
 * +-- public: run(), quit(), virtual hooks
 *     (Everyone can access)
 * ```
 *
 * INTERFACE ABSTRACTION EVOLUTION:
 *
 * September 23, 2025 (Initial - Direct OpenGL):
 * ```cpp
 * // Direct OpenGL calls in Application
 * glEnable(GL_DEPTH_TEST);
 * glEnable(GL_CULL_FACE);
 * glEnable(GL_MULTISAMPLE);
 * ```
 * Problem: Hardcoded to OpenGL (Vulkan incompatible)
 *
 * November 7, 2025 (IRenderDevice Integration):
 * ```cpp
 * // Factory pattern for resource creation
 * m_renderDevice = std::make_unique<GLRenderDevice>();
 * auto shader = m_renderDevice->createShaderFromFiles(...);
 * ```
 * Progress: Resource creation abstracted, but state management still OpenGL
 *
 * November 14, 2025 (IRenderer State Management):
 * ```cpp
 * // No OpenGL calls in Application
 * m_renderer = std::make_unique<GLRenderer>();
 * m_renderer->setDepthTest(true);
 * m_renderer->setFaceCulling(true);
 * ```
 * Result: Fully API-agnostic (same code works with VKRenderer)
 *
 * CURRENT STATE (November 14, 2025):
 * - Configuration-driven initialization (extensible, readable)
 * - Virtual hooks for user customization (clean override points)
 * - Careful initialization order (dependencies resolved)
 * - Protected accessors for derived classes (window, device, renderer)
 * - Interface abstraction (IRenderDevice, IRenderer, November 7-14)
 * - No direct OpenGL calls (November 14, API-agnostic)
 * - MSAA support (configurable)
 * - Material batching integrated (98% reduction)
 * - Transparency support (dual-queue rendering)
 * - Status: Production-ready, Vulkan-ready architecture
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Application Instance:
 * - Only one Application can exist (singleton pattern)
 * - Acceptable: Games typically single-instance
 *
 * 2. Config Immutable After Construction:
 * - Can't change config at runtime (by design)
 * - Use setters for runtime changes (renderer->setWireframeMode)
 *
 * 3. No Multi-Window Support:
 * - Single window only
 * - Future: Multiple windows
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 23, 2025: Initial implementation
 * - Base Application class (virtual hooks)
 * - Configuration structs (extensible, readable)
 * - Initialization order (carefully sequenced)
 * - Main loop (variable + fixed timestep)
 * - Direct OpenGL calls (not abstracted yet)
 *
 * (October 25-28, 2025): Rendering features
 * - MSAA configuration (msaaSamples)
 * - Material batching (98% reduction)
 * - Transparency support (dual-queue)
 * - Post-processing foundation (framebuffers)
 *
 * November 7, 2025: IRenderDevice integration
 * - Replaced direct resource creation with factory
 * - GLRenderDevice implementation
 * - Resource managers use interface
 * - State management still OpenGL
 *
 * November 14, 2025: IRenderer state management
 * - Removed all OpenGL calls from Application
 * - GLRenderer for state management
 * - Fully API-agnostic (ready for Vulkan)
 * - Result: Complete interface abstraction
 *
 * Future (Vulkan):
 * - VKRenderDevice, VKRenderer implementations
 * - Same Application code (no changes needed)
 *
 * DEPENDENCIES:
 * - <memory>: std::unique_ptr (RAII resource management)
 * - <string>: std::string (config strings)
 * - core/Window.h: Window management
 * - events/Event.h: Event system
 * - core/Logger.h: Logging system
 * - ui/ImGuiLayer.h: UI framework
 * - renderer/interface/IRenderDevice.h: Resource factory (November 7)
 * - renderer/interface/IRenderer.h: State management (November 14)
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Single-threaded design
 * - All operations on main thread (game loop)
 *
 * REFERENCES:
 * - Game Engine Architecture 3rd Ed.: Application framework
 * - The Cherno's Game Engine Series: Application design
 *
 * HISTORY:
 * September 23, 2025: Initial implementation
 * - Base Application class (entry point for user apps)
 * - Configuration structs (ApplicationConfig, WindowConfig, RenderConfig)
 * - Virtual hooks (onInitialize, onUpdate, onRender, etc.)
 * - Careful initialization order (Logger, Window, Systems)
 * - Main loop (variable + fixed timestep)
 * - Direct OpenGL calls (not abstracted)
 *
 * October 25-28, 2025: Rendering features
 * - MSAA configuration (msaaSamples in RenderConfig)
 * - Material batching integration (98% reduction)
 * - Transparency support (dual-queue rendering)
 * - Post-processing foundation (framebuffers)
 *
 * November 7, 2025: IRenderDevice integration
 * - Replaced direct OpenGL resource creation with factory pattern
 * - m_renderDevice = std::make_unique<GLRenderDevice>()
 * - Resource managers initialized with render device
 * - State management still direct OpenGL
 * - Progress: Resource abstraction complete
 *
 * November 14, 2025: IRenderer state management
 * - Removed ALL OpenGL calls from Application
 * - m_renderer = std::make_unique<GLRenderer>()
 * - State management through interface (setDepthTest, setFaceCulling)
 * - Result: Fully API-agnostic Application class
 * - Ready for Vulkan (same code, different implementation)
 *
 */

namespace Engine
{

    class Application
    {
    public:
        enum class Mode
        {
            Direct,
            World,
            Editor
        };

        struct ApplicationConfig
        {
            Mode mode;
            LogLevel logLevel;
            std::string logFile;

            ApplicationConfig(
                Mode mode = Mode::Direct,
                LogLevel logLevel = LogLevel::Info,
                const std::string& logFile = "engine.log"
            )
                : mode(mode)
                , logLevel(logLevel)
                , logFile(logFile)
            {
            }
        };

        struct WindowConfig
        {
            std::string title;
            int width;
            int height;
            bool vsync;
            bool fullscreen;
            bool resizable;

            WindowConfig(
                const std::string& title = "3D Engine",
                int width = 1280,
                int height = 720,
                bool vsync = true,
                bool fullscreen = false,
                bool resizable = true
            )
                : title(title)
                , width(width)
                , height(height)
                , vsync(vsync)
                , fullscreen(fullscreen)
                , resizable(resizable)
            {
            }
        };

        struct RenderConfig
        {
            bool wireframeMode;
            bool showDebugRenderer;
            bool enableFaceCulling;
            bool enableDepthTest;
            int anisotropicFiltering;
            int msaaSamples;

            RenderConfig(
                bool wireframeMode = false,
                bool showDebugRenderer = true,
                bool enableFaceCulling = false,
                bool enableDepthTest = true,
                int anisotropicFiltering = 16,
                int msaaSamples = 4
            )
                : wireframeMode(wireframeMode)
                , showDebugRenderer(showDebugRenderer)
                , enableFaceCulling(enableFaceCulling)
                , enableDepthTest(enableDepthTest)
                , anisotropicFiltering(anisotropicFiltering)
                , msaaSamples(msaaSamples)
            {
            }
        };

        int getAnisotropicFiltering() const;

        struct Config
        {
            ApplicationConfig app;
            WindowConfig window;
            RenderConfig render;
        };

        Application(const Config& config = {});
        virtual ~Application();

        // Core lifecycle hooks for user
        virtual void onInitialize() {}
        virtual void onUpdate(float deltaTime) {} // Variable Timestep
        virtual void onFixedUpdate(float fixedDeltaTime) {} // Fixed Timestep 60hz
        virtual void onRender() {}
        virtual void onShutdown() {}
        virtual void onEvent(Event& event) {}

        // Main entry point
        void run();
        void quit() { m_running = false; }

    protected:
        // Allow derived classes to access window and render device
        Window* getWindow() { return m_window.get(); }
        IRenderDevice* getRenderDevice() { return m_renderDevice.get(); }
        IRenderer* getRenderer() { return m_renderer.get(); }

    private:
        void initializeCore();
        void shutdownCore();

    private:
        Config m_config;
        Mode m_mode;
        bool m_running = true;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<IRenderDevice> m_renderDevice;
        std::unique_ptr<IRenderer> m_renderer;

        // Layers
        ImGuiLayer m_imguiLayer;

        // Singleton for global access
        static Application* s_instance;
    };

    // To be defined by user
    extern Application* createApplication();
}