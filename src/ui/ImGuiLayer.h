#pragma once
#include <SDL3/SDL.h>

namespace Engine
{
    // Forward declaration
    class Window;

/*
 * ImGuiLayer.h
 *
 * PURPOSE:
 * Wraps Dear ImGui initialization, rendering, and lifecycle management. Provides clean
 * interface between engine and ImGui library. Immediate-mode UI for fast iteration and
 * debugging. Minimal by design (only essential UI, grows as needed).
 *
 * DESIGN RATIONALE (October 2, 2025, Interface Refactor November 7-14, 2025):
 * Problem: Need debug UI (FPS, performance, state inspection). Traditional UI frameworks
 * complex (state management, event handling). Need fast iteration (add/remove UI easily).
 * Need input integration (keyboard/mouse capture).
 *
 * Solution: Dear ImGui integration with SDL3 + OpenGL backend.
 * - Immediate-mode: No state management (declare UI each frame)
 * - Fast iteration: Add UI with code (no visual editors)
 * - Minimal by design: Only essential UI (grows as needed)
 * - Interface refactor: renderDebugUI() adapted for IRenderDevice (November 7-14)
 * - Result: Clean debug UI, fast development, minimal overhead
 *
 * Key Insight: ImGui perfect for debug UI (immediate-mode = fast iteration). No complex
 * state management (declare UI each frame, simple). Minimal philosophy (only build
 * necessary UI, project immature). Interface refactor required changes to renderDebugUI()
 * (November 7-14, adapted for IRenderDevice). Small incremental changes (UI added as
 * needed, not upfront).
 *
 * DESIGN PHILOSOPHY:
 * - Immediate-mode: Declare UI each frame (no retained state)
 * - Fast iteration: Add/remove UI quickly (code-based)
 * - Minimal by design: Only essential UI (grows as needed)
 * - Clean integration: Wraps ImGui complexity
 * - Input priority: Game > UI (check capture state)
 *
 * KEY CONCEPTS:
 * 1. Immediate-Mode UI:
 *    - Declare UI each frame (no state retention)
 *    - Example: if (ImGui::Button("Click")) { action(); }
 *    - Simple: No event handling, no callbacks
 *
 * 2. Frame Lifecycle:
 *    - beginFrame(): Start ImGui frame (new frame context)
 *    - [User code declares UI]
 *    - render(): Render ImGui draw data (OpenGL calls)
 *
 * 3. Event Processing:
 *    - processEvent(): Feed SDL events to ImGui
 *    - ImGui updates internal state (mouse, keyboard)
 *    - Check capture state (wantsCaptureMouse, wantsCaptureKeyboard)
 *
 * 4. Input Priority:
 *    - UI captures input (when mouse over UI, typing in textbox)
 *    - Game checks capture state (skip input if captured)
 *    - Example: if (!imguiLayer.wantsCaptureMouse()) { handleGameInput(); }
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION ===
 * ImGuiLayer imguiLayer;
 * imguiLayer.init(&window);
 *
 * // Register event callback (raw SDL events for ImGui)
 * window.setRawEventCallback([&](const SDL_Event& e) {
 *     imguiLayer.processEvent(e);
 * });
 *
 * // === MAIN LOOP ===
 * while (running) {
 *     // Poll events (ImGui processes via callback)
 *     window.pollEvents();
 *
 *     // Begin ImGui frame
 *     imguiLayer.beginFrame();
 *
 *     // Game logic (check input capture)
 *     if (!imguiLayer.wantsCaptureKeyboard()) {
 *         handleGameInput();
 *     }
 *
 *     // Game rendering
 *     scene.render();
 *
 *     // Render ImGui (debug UI, overlays)
 *     imguiLayer.render();
 *
 *     // Present
 *     window.swapBuffers();
 * }
 *
 * // === CLEANUP ===
 * imguiLayer.shutdown();
 *
 * // === CUSTOM DEBUG UI ===
 * void renderDebugUI() {
 *     if (ImGui::Begin("Debug Info")) {
 *         // FPS display
 *         ImGui::Text("FPS: %.1f", EngineTime::getFPS());
 *
 *         // Frame time
 *         float frameMs = EngineTime::getDeltaTime() * 1000.0f;
 *         ImGui::Text("Frame: %.2fms", frameMs);
 *
 *         // Camera info
 *         ImGui::Text("Camera: %s", camera.position);
 *
 *         // Toggles
 *         static bool wireframe = false;
 *         if (ImGui::Checkbox("Wireframe", &wireframe)) {
 *             renderer->setWireframeMode(wireframe);
 *         }
 *
 *         ImGui::End();
 *     }
 * }
 *
 * // === TOGGLE DEBUG UI (F1 KEY) ===
 * void onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed && e.keyboard.key == SDLK_F1) {
 *         imguiLayer.toggleDebugUI();
 *     }
 * }
 * ```
 *
 * FRAME LIFECYCLE - Detailed Flow:
 *
 * ```cpp
 * void Application::run() {
 *     while (running) {
 *         // 1. POLL EVENTS
 *         window.pollEvents();
 *         // - SDL_PollEvent() called
 *         // - Raw events sent to ImGui (processEvent)
 *         // - ImGui updates internal state
 *
 *         // 2. BEGIN IMGUI FRAME
 *         imguiLayer.beginFrame();
 *         // - ImGui_ImplSDL3_NewFrame()
 *         // - ImGui_ImplOpenGL3_NewFrame()
 *         // - ImGui::NewFrame()
 *         // - Creates new frame context (reset draw data)
 *
 *         // 3. GAME LOGIC + UI DECLARATION
 *         update(deltaTime);
 *         // - Game code declares UI (immediate-mode)
 *         // - Example: ImGui::Button("Click Me")
 *         // - UI recorded in ImGui draw lists
 *
 *         // 4. GAME RENDERING
 *         scene.render();
 *         // - Game renders to framebuffer/screen
 *
 *         // 5. RENDER IMGUI
 *         imguiLayer.render();
 *         // - ImGui::Render()
 *         // - ImGui_ImplOpenGL3_RenderDrawData()
 *         // - Converts draw lists to OpenGL calls
 *         // - Renders UI overlay on top of game
 *
 *         // 6. PRESENT
 *         window.swapBuffers();
 *     }
 * }
 * ```
 *
 * EVENT PROCESSING - SDL to ImGui:
 *
 * ```cpp
 * void ImGuiLayer::processEvent(const SDL_Event& event) {
 *     // Feed SDL event to ImGui backend
 *     ImGui_ImplSDL3_ProcessEvent(&event);
 *
 *     // ImGui updates internal state:
 *     // - Mouse position (SDL_MOUSEMOTION)
 *     // - Mouse buttons (SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP)
 *     // - Keyboard (SDL_KEYDOWN, SDL_KEYUP)
 *     // - Text input (SDL_TEXTINPUT)
 *     // - Mouse wheel (SDL_MOUSEWHEEL)
 * }
 * ```
 *
 * INPUT CAPTURE - Game vs UI Priority:
 *
 * Problem: UI and game both want input
 * - Mouse over button: UI should capture (game ignores)
 * - Typing in textbox: UI should capture (game ignores)
 * - Mouse over game world: Game should capture (UI ignores)
 *
 * Solution: Query ImGui capture state
 * ```cpp
 * void handleInput() {
 *     // Check if ImGui wants keyboard
 *     if (!imguiLayer.wantsCaptureKeyboard()) {
 *         // Safe to process game keyboard input
 *         if (Input::isKeyPressed(SDLK_W)) {
 *             player.moveForward();
 *         }
 *     }
 *
 *     // Check if ImGui wants mouse
 *     if (!imguiLayer.wantsCaptureMouse()) {
 *         // Safe to process game mouse input
 *         if (Input::isMouseButtonPressed(MouseButton::Left)) {
 *             weapon.fire();
 *         }
 *     }
 * }
 * ```
 *
 * DEBUG UI - Minimal by Design:
 *
 * Current implementation (minimal):
 * ```cpp
 * void renderDebugUI() {
 *     if (!m_showDebugUI) return;
 *
 *     ImGui::Begin("Debug");
 *     ImGui::Text("FPS: %.1f", EngineTime::getFPS());
 *     ImGui::Text("Frame: %.2fms", EngineTime::getDeltaTime() * 1000.0f);
 *     ImGui::End();
 * }
 * ```
 *
 * Philosophy: Only essential UI (grows as needed)
 * - FPS counter: Performance monitoring
 * - Frame time: Performance profiling
 * - Add UI as needed: Scene hierarchy, material editor, etc.
 *
 * Future expansions (as needed):
 * - Scene hierarchy (object inspector)
 * - Material editor (texture, color tweaking)
 * - Performance profiler (hierarchical timing)
 * - Console (command input, logs)
 * - Asset browser (textures, models)
 *
 * IMMEDIATE-MODE UI - How It Works:
 *
 * Traditional UI (retained mode):
 * ```cpp
 * // Setup (once)
 * Button button("Click Me");
 * button.onClick = []() { action(); };
 *
 * // Update loop
 * button.update();
 * if (button.isClicked()) {
 *     button.onClick();
 * }
 * ```
 * Complex: State management, event handling, callbacks
 *
 * Immediate-mode UI (ImGui):
 * ```cpp
 * // Update loop (declare every frame)
 * if (ImGui::Button("Click Me")) {
 *     action();
 * }
 * ```
 * Simple: No state, no callbacks, inline logic
 *
 * INITIALIZATION - Backend Setup:
 *
 * ```cpp
 * void ImGuiLayer::init(Window* window) {
 *     m_window = window;
 *
 *     // 1. Create ImGui context
 *     IMGUI_CHECKVERSION();
 *     ImGui::CreateContext();
 *
 *     // 2. Configure ImGui
 *     ImGuiIO& io = ImGui::GetIO();
 *     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard nav
 *
 *     // 3. Set style (dark theme)
 *     ImGui::StyleColorsDark();
 *
 *     // 4. Initialize backends
 *     ImGui_ImplSDL3_InitForOpenGL(window->getNativeWindow(),
 *                                   window->getGLContext());
 *     ImGui_ImplOpenGL3_Init("#version 460");  // OpenGL 4.6
 *
 *     m_initialized = true;
 * }
 * ```
 *
 * INTERFACE REFACTOR IMPACT (November 7-14, 2025):
 *
 * Before (Direct OpenGL):
 * ```cpp
 * void renderDebugUI() {
 *     // Direct OpenGL calls, hardcoded resources
 *     ImGui::Text("Vertices: %d", mesh->getVertexCount());
 * }
 * ```
 *
 * After (IRenderDevice):
 * ```cpp
 * void renderDebugUI() {
 *     // Use interface for resource queries
 *     ImGui::Text("Vertices: %d", mesh->getVertexCount());
 *     // Same code, but mesh now IMesh* (interface)
 * }
 * ```
 *
 * Changes required:
 * - Minimal: ImGui code mostly unchanged
 * - Resource queries: Use interface methods (getVertexCount, etc.)
 * - No OpenGL calls: ImGui backend handles rendering
 *
 * CURRENT STATE (November 14, 2025):
 * - Dear ImGui integration (SDL3 + OpenGL3 backend)
 * - Immediate-mode UI (declare each frame)
 * - Frame lifecycle management (beginFrame, render)
 * - Event processing (SDL -> ImGui)
 * - Input capture queries (wantsCaptureMouse, wantsCaptureKeyboard)
 * - Debug UI (FPS, frame time, minimal)
 * - Toggle controls (F1 key, setDebugUIVisible)
 * - Interface compatible (works with IRenderDevice, November 7-14)
 * - Status: Production-ready, minimal by design
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Minimal UI:
 * - Only basic debug info (FPS, frame time)
 * - Intentional: Project immature, add as needed
 * - Future: Scene hierarchy, material editor, profiler (as needed)
 *
 * 2. Single Window:
 * - All UI in one debug window
 * - Future: Multiple panels, docking (when needed)
 *
 * 3. No Serialization:
 * - Can't save UI layout, preferences
 * - Future: Save/load layout 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 2, 2025: Initial implementation
 * - Dear ImGui integration (SDL3 + OpenGL3)
 * - Basic lifecycle (init, beginFrame, render, shutdown)
 * - Event processing (processEvent)
 * - Minimal debug UI (FPS counter)
 * - Status: Basic functionality
 *
 * Ongoing: Minimal additions
 * - Frame time display
 * - Input capture queries
 * - Toggle controls (F1 key)
 * - Demo window toggle
 * - Philosophy: Add only when needed
 *
 * November 7-14, 2025: Interface refactor adaptation
 * - renderDebugUI() adapted for IRenderDevice
 * - Resource queries through interfaces
 * - No major changes (ImGui mostly unaffected)
 *
 * Future (As Needed):
 * - Scene hierarchy 
 * - Material editor 
 * - Performance profiler 
 * - Console
 * - Asset browser
 *
 * DEPENDENCIES:
 * - <SDL3/SDL.h>: SDL3 event types
 * - Dear ImGui: UI library (external)
 * - ImGui backends: SDL3, OpenGL3
 *
 * THREAD SAFETY:
 * - NOT thread-safe: ImGui is single-threaded
 * - All operations on main thread (render thread)
 *
 * REFERENCES:
 * - Dear ImGui documentation: https://github.com/ocornut/imgui
 * - ImGui examples: SDL3 + OpenGL3 backend
 *
 * HISTORY:
 * October 2, 2025: Initial implementation
 * - Dear ImGui integration (SDL3 + OpenGL3 backend)
 * - Basic lifecycle management (init, shutdown)
 * - Frame management (beginFrame, render)
 * - Event processing (processEvent, SDL -> ImGui)
 * - Input capture queries (wantsCaptureMouse, wantsCaptureKeyboard)
 * - Minimal debug UI (FPS counter, frame time)
 * - Toggle controls (setDebugUIVisible, toggleDebugUI)
 *
 * Ongoing: Incremental additions
 * - Demo window toggle (learning/testing)
 * - Additional debug info (as needed)
 * - Small UI improvements (minimal, practical)
 * - Philosophy: Only build necessary UI (project immature)
 *
 * November 7-14, 2025: Interface refactor adaptation
 * - renderDebugUI() adapted for IRenderDevice/IRenderer
 * - Resource queries through interfaces (IMesh*, ITexture*)
 * - No major changes (ImGui backend handles OpenGL)
 * - Minimal impact (UI mostly unchanged)
 *
 */

    class ImGuiLayer
    {
    public:
        ImGuiLayer() = default;
        ~ImGuiLayer() = default;

        // Lifecycle
        void init(Window* window);
        void shutdown();

        // Frame management
        void beginFrame();
        void render();

        // Event processing
        void processEvent(const SDL_Event& event);

        // Input capture queries
        bool wantsCaptureMouse() const;
        bool wantsCaptureKeyboard() const;

        // Debug UI control
        void setDebugUIVisible(bool visible) { m_showDebugUI = visible; }
        bool isDebugUIVisible() const { return m_showDebugUI; }
        void toggleDebugUI() { m_showDebugUI = !m_showDebugUI; }

        // Demo window control
        void setDemoWindowVisible(bool visible) { m_showDemoWindow = visible; }
        void toggleDemoWindow() { m_showDemoWindow = !m_showDemoWindow; }

    private:
        // Internal rendering methods
        void renderDebugUI();

        // State
        bool m_initialized = false;
        bool m_showDebugUI = true;  // Default to visible in debug builds
        bool m_showDemoWindow = false; // Toggle for demo 
        Window* m_window = nullptr;  // Keep reference for context
    };
}