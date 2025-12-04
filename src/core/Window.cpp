#include "core/Window.h"
#include "core/Logger.h"
#include <glad/glad.h>
#include <iostream>

namespace Engine
{
    Window::Window(const Properties& props)
        : m_properties(props)
    {
        m_isValid = init();
        if (!m_isValid)
        {
            LOG_ERROR("Window construction failed");
        }
    }

    Window::~Window()
    {
        shutdown();
    }

    bool Window::init()
    {
        // Initialize SDL (common for all APIs)
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        {
            LOG_ERROR("Failed to initialize SDL: {}", SDL_GetError());
            return false;
        }

        // Dispatch to API-specific initialization
        switch (m_properties.api)
        {
        case GraphicsAPI::OpenGL:
            LOG_INFO("Initializing window with OpenGL context");
            return initOpenGL();

        case GraphicsAPI::Vulkan:
            LOG_INFO("Initializing window with Vulkan surface");
            return initVulkan();

        default:
            LOG_ERROR("Unknown graphics API requested");
            return false;
        }
    }

    bool Window::initOpenGL()
    {
        // Set OpenGL attributes BEFORE window creation
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // Create window flags (OpenGL-specific)
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (m_properties.resizable) windowFlags |= SDL_WINDOW_RESIZABLE;
        if (m_properties.fullscreen) windowFlags |= SDL_WINDOW_FULLSCREEN;

        // Create window
        m_window = SDL_CreateWindow(
            m_properties.title.c_str(),
            m_properties.width,
            m_properties.height,
            windowFlags
        );

        if (!m_window)
        {
            LOG_ERROR("Failed to create OpenGL window: {}", SDL_GetError());
            SDL_Quit();
            return false;
        }

        // Create OpenGL context
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext)
        {
            LOG_ERROR("Failed to create OpenGL context: {}", SDL_GetError());
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        // Make context current
        SDL_GL_MakeCurrent(m_window, m_glContext);

        // Set VSync (OpenGL-specific)
        int swapInterval = m_properties.vsync ? 1 : 0;
        if (!SDL_GL_SetSwapInterval(swapInterval))
        {
            LOG_WARN("Failed to set VSync to {}: {}",
                m_properties.vsync ? "ON" : "OFF",
                SDL_GetError());
            m_properties.vsync = false;
        }
        else
        {
            LOG_INFO("VSync {} (Note: GPU driver settings may override this)",
                m_properties.vsync ? "enabled" : "disabled");
        }

        // Initialize GLAD (OpenGL function loader)
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            LOG_ERROR("Failed to initialize GLAD");
            SDL_GL_DestroyContext(m_glContext);
            SDL_DestroyWindow(m_window);
            SDL_Quit();
            return false;
        }

        LOG_INFO("OpenGL window created: {} ({}x{})",
            m_properties.title, m_properties.width, m_properties.height);

        // Log OpenGL version for verification
        const GLubyte* version = glGetString(GL_VERSION);
        LOG_INFO("OpenGL Version: {}", reinterpret_cast<const char*>(version));

        return true;
    }

    bool Window::initVulkan()
    {
        // FUTURE IMPLEMENTATION (Week 20+)
        LOG_ERROR("Vulkan support not yet implemented");
        LOG_ERROR("Please use GraphicsAPI::OpenGL for now");
        SDL_Quit();
        return false;

        /* FUTURE CODE (when implementing Vulkan):

        // Create window flags (Vulkan-specific)
        Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (m_properties.resizable) windowFlags |= SDL_WINDOW_RESIZABLE;
        if (m_properties.fullscreen) windowFlags |= SDL_WINDOW_FULLSCREEN;

        // Create window
        m_window = SDL_CreateWindow(
            m_properties.title.c_str(),
            m_properties.width,
            m_properties.height,
            windowFlags
        );

        if (!m_window)
        {
            LOG_ERROR("Failed to create Vulkan window: {}", SDL_GetError());
            SDL_Quit();
            return false;
        }

        // Vulkan surface creation will be handled by VKRenderDevice
        // (Window just creates the SDL_Window, VK instance creates surface)

        LOG_INFO("Vulkan window created: {} ({}x{})",
            m_properties.title, m_properties.width, m_properties.height);

        return true;
        */
    }

    void Window::shutdown()
    {
        // API-specific cleanup
        if (m_properties.api == GraphicsAPI::OpenGL && m_glContext)
        {
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }

        // Common cleanup
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
    }

    void Window::pollEvents()
    {
        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent))
        {
            // Call raw event callback for ImGui
            if (m_rawEventCallback)
            {
                m_rawEventCallback(sdlEvent);
            }

            // Then translate for engine
            translateSDLEvent(sdlEvent);
        }
    }

    void Window::swapBuffers()
    {
        // API-specific buffer swap
        if (m_properties.api == GraphicsAPI::OpenGL)
        {
            SDL_GL_SwapWindow(m_window);
        }
        // Vulkan swapchain present will be handled by VKRenderDevice
    }

    void Window::translateSDLEvent(const SDL_Event& sdlEvent)
    {
        if (!m_eventCallback) return;

        Event event;

        switch (sdlEvent.type)
        {
        case SDL_EVENT_QUIT:
            event.type = EventType::WindowClose;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            event.type = EventType::WindowResize;
            event.size.width = sdlEvent.window.data1;
            event.size.height = sdlEvent.window.data2;
            m_properties.width = event.size.width;
            m_properties.height = event.size.height;

            // Only call glViewport for OpenGL (Vulkan handles differently)
            if (m_properties.api == GraphicsAPI::OpenGL)
            {
                glViewport(0, 0, event.size.width, event.size.height);
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            event.type = EventType::KeyPressed;
            event.keyboard.key = sdlEvent.key.key;
            event.keyboard.scancode = sdlEvent.key.scancode;
            event.keyboard.repeat = sdlEvent.key.repeat;
            event.keyboard.shift = (sdlEvent.key.mod & SDL_KMOD_SHIFT) != 0;
            event.keyboard.ctrl = (sdlEvent.key.mod & SDL_KMOD_CTRL) != 0;
            event.keyboard.alt = (sdlEvent.key.mod & SDL_KMOD_ALT) != 0;
            break;

        case SDL_EVENT_KEY_UP:
            event.type = EventType::KeyReleased;
            event.keyboard.key = sdlEvent.key.key;
            event.keyboard.scancode = sdlEvent.key.scancode;
            event.keyboard.repeat = false;
            event.keyboard.shift = (sdlEvent.key.mod & SDL_KMOD_SHIFT) != 0;
            event.keyboard.ctrl = (sdlEvent.key.mod & SDL_KMOD_CTRL) != 0;
            event.keyboard.alt = (sdlEvent.key.mod & SDL_KMOD_ALT) != 0;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            event.type = EventType::MouseMoved;
            event.mouseMove.x = sdlEvent.motion.x;
            event.mouseMove.y = sdlEvent.motion.y;

            // Use relative motion if in relative mode
            if (SDL_GetWindowRelativeMouseMode(m_window))
            {
                event.mouseMove.deltaX = sdlEvent.motion.xrel;
                event.mouseMove.deltaY = sdlEvent.motion.yrel;
            }
            else
            {
                event.mouseMove.deltaX = event.mouseMove.x - m_lastMouseX;
                event.mouseMove.deltaY = event.mouseMove.y - m_lastMouseY;
            }
            m_lastMouseX = event.mouseMove.x;
            m_lastMouseY = event.mouseMove.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            event.type = EventType::MouseButtonPressed;
            event.mouseButton.button = sdlEvent.button.button - 1;  // SDL uses 1-based
            event.mouseButton.x = sdlEvent.button.x;
            event.mouseButton.y = sdlEvent.button.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            event.type = EventType::MouseButtonReleased;
            event.mouseButton.button = sdlEvent.button.button - 1;
            event.mouseButton.x = sdlEvent.button.x;
            event.mouseButton.y = sdlEvent.button.y;
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            event.type = EventType::MouseScrolled;
            event.scroll.deltaX = sdlEvent.wheel.x;
            event.scroll.deltaY = sdlEvent.wheel.y;
            break;

        default:
            return; // Don't send unknown events
        }

        m_eventCallback(event);
    }
}