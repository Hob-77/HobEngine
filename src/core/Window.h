#pragma once
#include <string>
#include <functional>
#include <SDL3/SDL.h>
#include "events/Event.h"

/*
 * Window.h
 *
 * PURPOSE:
 * Platform-independent window management with configurable graphics API support. Abstracts
 * SDL3 windowing system for both OpenGL and Vulkan (future). Handles window lifecycle,
 * event polling, and rendering context management. RAII resource management (automatic
 * cleanup, exception-safe).
 *
 * DESIGN RATIONALE (September 23, 2025, Updated November 15, 2025):
 * Problem: Need window abstraction (hide SDL3 complexity). Need graphics context (OpenGL
 * now, Vulkan later). Need clean event delivery (SDL -> Engine). Need automatic cleanup
 * (RAII, prevent leaks). Need flexible configuration (avoid constructor bloat).
 *
 * Solution: Window class with API-specific initialization paths.
 * - Properties struct: Clean configuration (extensible, self-documenting)
 * - GraphicsAPI enum: Select OpenGL or Vulkan (November 15 update)
 * - API-specific init: initOpenGL(), initVulkan() (isolated code paths)
 * - RAII: Automatic cleanup (reverse order, exception-safe)
 * - Dual callbacks: Translated events (game) + raw SDL (ImGui)
 * - Result: Flexible, maintainable, ready for Vulkan
 *
 * Key Insight: Window management fundamental to engines (every app needs window). RAII
 * critical (cleanup complex, error-prone if manual). Properties pattern cleaner than
 * constructor parameters (extensible, readable). API-specific init functions isolate
 * GL/Vulkan code (easy to maintain, test). Dual callbacks essential (ImGui needs raw
 * SDL, game needs translated). November 15 refactor: Prepare for Vulkan (separate init
 * paths, minimal future refactoring).
 *
 * DESIGN PHILOSOPHY:
 * - RAII: Automatic cleanup (constructor allocates, destructor frees)
 * - Properties pattern: Clean configuration (named fields, extensible)
 * - API-specific init: Isolated code paths (OpenGL, Vulkan)
 * - Callback pattern: Decouple Window from Application (no circular deps)
 * - Hide SDL: Game code never touches SDL directly
 *
 * KEY CONCEPTS:
 * 1. RAII (Resource Acquisition Is Initialization):
 *    - Constructor: SDL_Init, SDL_CreateWindow, initOpenGL/initVulkan
 *    - Destructor: Cleanup in reverse order (automatic, exception-safe)
 *    - No manual cleanup needed (no leaks, no dangling pointers)
 *
 * 2. Properties Pattern:
 *    - Configuration via struct (not constructor parameters)
 *    - Self-documenting (named fields)
 *    - Extensible (add fields without breaking code)
 *    - C++20 designated initializers (readable)
 *
 * 3. API-Specific Initialization (November 15):
 *    - initOpenGL(): OpenGL 4.6 Core, GLAD, VSync
 *    - initVulkan(): Vulkan surface, validation layers (future)
 *    - Selected via GraphicsAPI enum
 *
 * 4. Dual Callback System:
 *    - EventCallback: Translated events (game code)
 *    - RawEventCallback: Raw SDL events (ImGui)
 *    - Both coexist cleanly
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === BASIC WINDOW (OPENGL) ===
 * Window window({
 *     .title = "My Game",
 *     .width = 1920,
 *     .height = 1080,
 *     .vsync = true,
 *     .api = GraphicsAPI::OpenGL
 * });
 *
 * if (!window.isValid()) {
 *     LOG_FATAL("Failed to create window");
 *     return -1;
 * }
 *
 * // === EVENT CALLBACKS ===
 * // Game code (translated events)
 * window.setEventCallback([](Event& e) {
 *     if (e.type == EventType::WindowClose) {
 *         shutdown();
 *     }
 * });
 *
 * // ImGui (raw SDL events)
 * window.setRawEventCallback([](const SDL_Event& e) {
 *     ImGui_ImplSDL3_ProcessEvent(&e);
 * });
 *
 * // === MAIN LOOP ===
 * while (running) {
 *     // Poll and deliver events
 *     window.pollEvents();
 *
 *     // Update and render
 *     update(deltaTime);
 *     render();
 *
 *     // Present
 *     window.swapBuffers();
 * }
 * // Automatic cleanup (destructor called)
 *
 * // === QUERY PROPERTIES ===
 * int width = window.getWidth();
 * int height = window.getHeight();
 * float aspect = window.getAspectRatio();
 * bool vsync = window.isVSync();
 * GraphicsAPI api = window.getAPI();
 *
 * // === FUTURE: VULKAN ===
 * Window window({
 *     .title = "My Game",
 *     .api = GraphicsAPI::Vulkan  // Select Vulkan
 * });
 *
 * // Window handles Vulkan surface creation
 * // No code changes in main loop!
 * ```
 *
 * GRAPHICS API SUPPORT - Switching APIs:
 *
 * OpenGL version (current):
 * ```cpp
 * Window window({.api = GraphicsAPI::OpenGL});
 * auto renderDevice = std::make_unique<GLRenderDevice>();
 *
 * // Window creates OpenGL context, initializes GLAD
 * ```
 *
 * Vulkan version (future):
 * ```cpp
 * Window window({.api = GraphicsAPI::Vulkan});
 * auto renderDevice = std::make_unique<VKRenderDevice>();
 *
 * // Window creates Vulkan surface, no GLAD
 * // Minimal code changes!
 * ```
 *
 * PROPERTIES PATTERN - Clean Configuration:
 *
 * Before (constructor parameters):
 * ```cpp
 * // Hard to read, hard to extend
 * Window window("My Game", 1920, 1080, true, false, true, GraphicsAPI::OpenGL);
 * // What does each parameter mean? (confusing)
 * ```
 *
 * After (Properties struct):
 * ```cpp
 * // Self-documenting, extensible
 * Window window({
 *     .title = "My Game",
 *     .width = 1920,
 *     .height = 1080,
 *     .vsync = true,
 *     .fullscreen = false,
 *     .resizable = true,
 *     .api = GraphicsAPI::OpenGL
 * });
 * // Clear what each field means (readable)
 * ```
 *
 * Benefits:
 * - Named fields (self-documenting)
 * - Default values (sensible fallbacks)
 * - Extensible (add fields without breaking code)
 * - C++20 designated initializers (clean syntax)
 *
 * RAII - Automatic Resource Management:
 *
 * Constructor (acquire resources):
 * ```cpp
 * Window::Window(const Properties& props) {
 *     // 1. Initialize SDL
 *     SDL_Init(SDL_INIT_VIDEO);
 *
 *     // 2. Create window
 *     m_window = SDL_CreateWindow(...);
 *
 *     // 3. Create graphics context (API-specific)
 *     if (props.api == GraphicsAPI::OpenGL) {
 *         initOpenGL();  // Create GL context, init GLAD
 *     } else {
 *         initVulkan();  // Create Vulkan surface (future)
 *     }
 *
 *     m_isValid = true;
 * }
 * ```
 *
 * Destructor (release resources, reverse order):
 * ```cpp
 * Window::~Window() {
 *     // 3. Destroy graphics context
 *     if (m_glContext) {
 *         SDL_GL_DestroyContext(m_glContext);
 *     }
 *
 *     // 2. Destroy window
 *     if (m_window) {
 *         SDL_DestroyWindow(m_window);
 *     }
 *
 *     // 1. Shutdown SDL
 *     SDL_Quit();
 * }
 * ```
 *
 * Benefits:
 * - Automatic cleanup (no manual delete, no leaks)
 * - Exception-safe (destructor always runs)
 * - Correct order (reverse of construction)
 *
 * API-SPECIFIC INITIALIZATION - OpenGL vs Vulkan:
 *
 * OpenGL initialization (current):
 * ```cpp
 * bool Window::initOpenGL() {
 *     // Set OpenGL attributes (before window creation)
 *     SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
 *     SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
 *     SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
 *                         SDL_GL_CONTEXT_PROFILE_CORE);
 *
 *     // Create OpenGL context
 *     m_glContext = SDL_GL_CreateContext(m_window);
 *     if (!m_glContext) return false;
 *
 *     // Make context current
 *     SDL_GL_MakeCurrent(m_window, m_glContext);
 *
 *     // Initialize GLAD (OpenGL function loader)
 *     if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
 *         return false;
 *     }
 *
 *     // Configure VSync
 *     SDL_GL_SetSwapInterval(m_properties.vsync ? 1 : 0);
 *
 *     return true;
 * }
 * ```
 *
 * Vulkan initialization (future):
 * ```cpp
 * bool Window::initVulkan() {
 *     // Create Vulkan surface (SDL handles platform differences)
 *     VkSurfaceKHR surface;
 *     if (!SDL_Vulkan_CreateSurface(m_window, instance, &surface)) {
 *         return false;
 *     }
 *
 *     // No GLAD loading (Vulkan uses different loader)
 *     // VSync handled through swapchain presentation mode
 *
 *     return true;
 * }
 * ```
 *
 * DUAL CALLBACK SYSTEM - ImGui + Game Code:
 *
 * Problem: ImGui needs raw SDL events
 * ```cpp
 * // ImGui processes raw SDL events
 * ImGui_ImplSDL3_ProcessEvent(&sdlEvent);
 * ```
 *
 * Problem: Game code wants translated events
 * ```cpp
 * // Game code wants engine Event (SDL-independent)
 * void onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed) { ... }
 * }
 * ```
 *
 * Solution: Two callback types
 * ```cpp
 * void Window::pollEvents() {
 *     SDL_Event sdlEvent;
 *     while (SDL_PollEvent(&sdlEvent)) {
 *         // 1. Raw callback (ImGui first)
 *         if (m_rawEventCallback) {
 *             m_rawEventCallback(sdlEvent);
 *         }
 *
 *         // 2. Translate SDL -> Engine Event
 *         translateSDLEvent(sdlEvent);
 *
 *         // 3. Translated callback (game code)
 *         if (m_eventCallback) {
 *             m_eventCallback(event);
 *         }
 *     }
 * }
 * ```
 *
 * WINDOW LIFECYCLE - Complete Flow:
 *
 * ```cpp
 * // 1. CONSTRUCTION
 * Window window({
 *     .title = "My Game",
 *     .width = 1920,
 *     .height = 1080,
 *     .vsync = true,
 *     .api = GraphicsAPI::OpenGL
 * });
 * // - SDL_Init(SDL_INIT_VIDEO)
 * // - SDL_CreateWindow(...)
 * // - initOpenGL() or initVulkan()
 *
 * // 2. CONFIGURATION
 * window.setEventCallback([](Event& e) { ... });
 * window.setRawEventCallback([](const SDL_Event& e) { ... });
 *
 * // 3. EVENT LOOP
 * while (running) {
 *     window.pollEvents();  // Poll and deliver events
 *     update(deltaTime);
 *     render();
 *     window.swapBuffers();  // Present
 * }
 *
 * // 4. DESTRUCTION (automatic)
 * // - SDL_GL_DestroyContext(m_glContext)
 * // - SDL_DestroyWindow(m_window)
 * // - SDL_Quit()
 * ```
 *
 * OPENGL CONTEXT - Details:
 *
 * OpenGL 4.6 Core profile:
 * - Modern pipeline (no fixed-function)
 * - Shader-based rendering only
 * - Direct State Access (DSA)
 * - Compute shaders, tessellation
 * - Geometry shaders, transform feedback
 *
 * Configuration:
 * ```cpp
 * // Version 4.6
 * SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
 * SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
 *
 * // Core profile (no deprecated features)
 * SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
 *                     SDL_GL_CONTEXT_PROFILE_CORE);
 *
 * // Double buffering (smooth rendering)
 * SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
 * ```
 *
 * VSYNC CONFIGURATION:
 *
 * OpenGL:
 * ```cpp
 * SDL_GL_SetSwapInterval(1);  // VSync on
 * SDL_GL_SetSwapInterval(0);  // VSync off
 * ```
 *
 * Vulkan (future):
 * ```cpp
 * // VSync via presentation mode
 * VK_PRESENT_MODE_FIFO_KHR;        // VSync (guaranteed)
 * VK_PRESENT_MODE_IMMEDIATE_KHR;   // No VSync
 * VK_PRESENT_MODE_MAILBOX_KHR;     // Triple buffering
 * ```
 *
 * Note: Driver settings may override (NVIDIA Control Panel, etc.)
 *
 * CURRENT STATE (November 15, 2025):
 * - RAII window management (automatic cleanup)
 * - Properties pattern configuration (extensible, readable)
 * - GraphicsAPI enum (OpenGL, Vulkan)
 * - API-specific initialization (initOpenGL, initVulkan stub)
 * - Dual callback system (translated + raw events)
 * - OpenGL 4.6 Core context (modern pipeline)
 * - VSync support (configurable)
 * - SDL3 abstraction (game code never touches SDL)
 * - Status: Production-ready, Vulkan-ready architecture
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Window Only:
 * - Can't create multiple windows (no multi-window support)
 * - Future: Multiple window support
 *
 * 2. VSync Reporting Inaccurate:
 * - isVSync() returns requested state (not actual)
 * - Driver may override (not detected)
 * - Future: Query actual VSync state
 *
 * 3. No DPI Awareness:
 * - High-DPI displays may have issues (scaling)
 * - Future: DPI awareness (SDL_WINDOW_ALLOW_HIGHDPI)
 *
 * 4. No Window State Queries:
 * - Can't query minimized/maximized state
 * - Future: getWindowState()
 *
 * 5. No Borderless Fullscreen:
 * - Only windowed or exclusive fullscreen
 * - Future: Borderless fullscreen mode
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 23, 2025: Initial implementation
 * - RAII window management
 * - Properties pattern configuration
 * - OpenGL context creation
 * - Event translation and callbacks
 * - SDL3 abstraction
 * - Status: Production-ready (OpenGL only)
 *
 * November 15, 2025: Graphics API refactor
 * - Added GraphicsAPI enum (OpenGL, Vulkan)
 * - Separated init paths (initOpenGL, initVulkan)
 * - Prepared for Vulkan (minimal future refactoring)
 * - Result: Vulkan-ready architecture
 *
 * (Vulkan Implementation):
 * - Implement initVulkan() (surface creation)
 * - Test dual-API support
 * - Time: 1-2 weeks
 *
 * (Multiple Windows):
 * - Support multiple window instances
 * - Editor tools, multi-monitor setups
 * - Time: 1 week
 *
 * (DPI Awareness):
 * - High-DPI display support
 * - SDL_WINDOW_ALLOW_HIGHDPI
 * - Time: 2-3 days
 *
 * (Window State Queries):
 * - Query minimized/maximized/focused state
 * - Optimize rendering when minimized
 * - Time: 1-2 days
 *
 * DEPENDENCIES:
 * - <string>: std::string (window title)
 * - <functional>: std::function (callbacks)
 * - <SDL3/SDL.h>: SDL3 (windowing, OpenGL context)
 * - events/Event.h: Engine event types
 *
 * THREAD SAFETY:
 * - NOT thread-safe: SDL operations on main thread only
 * - All window operations must be on main thread
 *
 * REFERENCES:
 * - SDL3 documentation: Window and OpenGL context
 * - Vulkan specification: Surface creation (future)
 *
 * HISTORY:
 * September 23, 2025: Initial implementation
 * - RAII window management (automatic cleanup, exception-safe)
 * - Properties pattern (clean configuration, extensible)
 * - OpenGL 4.6 Core context (modern pipeline)
 * - Event translation (SDL -> Engine)
 * - Dual callbacks (translated + raw events for ImGui)
 * - SDL3 abstraction (hide SDL from game code)
 *
 * November 15, 2025: Graphics API refactor
 * - Added GraphicsAPI enum (OpenGL, Vulkan selection)
 * - Separated initialization paths (initOpenGL, initVulkan stub)
 * - Isolated API-specific code (easy to maintain, test)
 * - Prepared for Vulkan (minimal future refactoring needed)
 * - Result: Flexible architecture, ready for multiple APIs
 *
 */

namespace Engine
{
    // Graphics API enum - determines context creation path
    enum class GraphicsAPI
    {
        OpenGL,   // Use OpenGL 4.6 Core (current)
        Vulkan    // Use Vulkan 1.3+ (future)
    };

    class Window
    {
    public:
        struct Properties
        {
            std::string title;
            int width;
            int height;
            bool vsync;
            bool fullscreen;
            bool resizable;
            GraphicsAPI api;

            // Constructor with defaults:
            Properties(
                const std::string& title = "3D Engine",
                int width = 1280,
                int height = 720,
                bool vsync = true,
                bool fullscreen = false,
                bool resizable = true,
                GraphicsAPI api = GraphicsAPI::OpenGL
            )
                : title(title)
                , width(width)
                , height(height)
                , vsync(vsync)
                , fullscreen(fullscreen)
                , resizable(resizable)
                , api(api)
            {
            }
        };

        using EventCallback = std::function<void(Event&)>;
        using RawEventCallback = std::function<void(const SDL_Event&)>;

        Window(const Properties& props = {});
        bool isValid() const { return m_isValid; }
        ~Window();

        // Main operations
        void pollEvents();
        void swapBuffers();

        // Properties
        int getWidth() const { return m_properties.width; }
        int getHeight() const { return m_properties.height; }
        float getAspectRatio() const { return (float)m_properties.width / (float)m_properties.height; }
        bool isVSync() const { return m_properties.vsync; }
        GraphicsAPI getAPI() const { return m_properties.api; }  // NEW: Query current API

        // Set callback that will receive translated events
        void setEventCallback(const EventCallback& callback) { m_eventCallback = callback; }

        // Set callback for raw SDL events for ImGui
        void setRawEventCallback(const RawEventCallback& callback) { m_rawEventCallback = callback; }

        // For OpenGL Renderer to get context if needed
        void* getGLContext() { return m_glContext; }

        // For ImGui Integration
        SDL_Window* getNativeWindow() { return m_window; }

    private:
        // Main initialization dispatcher
        bool init();
        void shutdown();

        // API-specific initialization
        bool initOpenGL();
        bool initVulkan();  // Future implementation

        // Event translation (API-agnostic)
        void translateSDLEvent(const SDL_Event& sdlEvent);

    private:
        bool m_isValid = false;
        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;  // Only used when api == OpenGL

        Properties m_properties;
        EventCallback m_eventCallback;
        RawEventCallback m_rawEventCallback;

        // Track mouse position for delta calculation
        float m_lastMouseX = 0.0f;
        float m_lastMouseY = 0.0f;
    };
}