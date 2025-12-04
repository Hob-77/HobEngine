#pragma once

/*
 * Event.h
 *
 * PURPOSE:
 * Temporal notification system for input, window state, and user-defined events. Bridges
 * SDL's event system with engine code through unified Event structure. Union-based design
 * for performance-critical core events, pointer-based for flexible user events.
 *
 * DESIGN RATIONALE (September 23, 2025):
 * Problem: Need fast event delivery for high-frequency events (keyboard, mouse, window).
 * Traditional approach: Virtual inheritance, heap allocation (slow, cache-unfriendly).
 * Thousands of events per frame = performance bottleneck. Need flexibility for game-
 * specific events (network, AI, physics). Need to hide SDL from game code.
 *
 * Solution: Hybrid event system (union + pointer).
 * - Core events: Union-based (stack-allocated, zero allocation, cache-friendly)
 * - User events: Pointer-based (heap-allocated, flexible, any data structure)
 * - SDL translation: Window::translateSDLEvent() converts SDL_Event to Event
 * - Result: Fast core events, flexible user events, clean abstraction
 *
 * Key Insight: Core events (keyboard, mouse, window) happen every frame (need speed).
 * User events (network, AI, game state) infrequent (need flexibility). Union eliminates
 * heap allocation for core events (Casey Muratori's Handmade Hero approach). Trade-off:
 * Harder to add new core event types (must update union), but performance gain worth it.
 *
 * DESIGN PHILOSOPHY:
 * - Union for core: Performance (no allocation, cache-friendly)
 * - Pointer for user: Flexibility (any data structure)
 * - Handled flag: Prevent double-processing (priority systems)
 * - SDL abstraction: Hide SDL from game code
 * - Zero-initialization: Prevent undefined behavior (union safety)
 *
 * KEY CONCEPTS:
 * 1. Union-Based Storage:
 *    - Core events share memory (only one active at a time)
 *    - Stack-allocated (no heap, no malloc, cache-friendly)
 *    - Performance: Critical for thousands of events per frame
 *
 * 2. Event Flow:
 *    - SDL generates SDL_Event
 *    - Window::translateSDLEvent() converts to Event
 *    - Window::eventCallback() delivers to Application
 *    - Application::onEvent() dispatches to subsystems
 *    - Input::onEvent() updates state, game handles rest
 *
 * 3. User Events:
 *    - Game-specific events (network, AI, physics)
 *    - Heap-allocated (flexible data structures)
 *    - Manual memory management (caller must delete)
 *
 * 4. Handled Flag:
 *    - Prevents multiple systems processing same event
 *    - Priority: UI > gameplay > debug
 *    - Mark handled to stop propagation
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === HANDLE CORE EVENTS ===
 * void Application::onEvent(Event& e) {
 *     switch (e.type) {
 *         case EventType::WindowClose:
 *             shutdown();
 *             break;
 *
 *         case EventType::WindowResize:
 *             renderer->resize(e.size.width, e.size.height);
 *             camera.setAspectRatio(e.size.width / (float)e.size.height);
 *             break;
 *
 *         case EventType::KeyPressed:
 *             if (e.keyboard.key == SDLK_ESCAPE && !e.handled) {
 *                 togglePauseMenu();
 *                 e.handled = true;  // Stop propagation
 *             }
 *             break;
 *
 *         case EventType::MouseMoved:
 *             // Use delta for camera rotation (FPS)
 *             camera.rotate(e.mouseMove.deltaX * sensitivity,
 *                          e.mouseMove.deltaY * sensitivity);
 *             break;
 *
 *         case EventType::MouseScrolled:
 *             camera.adjustZoom(e.scroll.deltaY);
 *             break;
 *     }
 *
 *     // Forward to input system
 *     Input::onEvent(e);
 * }
 *
 * // === CREATE USER EVENT ===
 * // Define custom event structure
 * struct NetworkEvent {
 *     std::string playerName;
 *     int ping;
 *     vec3 position;
 * };
 *
 * // Create and send event
 * Event e;
 * e.type = EventType::UserDefined;
 * e.userType = MY_NETWORK_EVENT;  // Custom enum
 * e.userData = new NetworkEvent{"Player1", 50, vec3(10, 0, 5)};
 *
 * application.onEvent(e);
 *
 * // Clean up (caller's responsibility)
 * delete static_cast<NetworkEvent*>(e.userData);
 *
 * // === HANDLE USER EVENT ===
 * void Game::onEvent(Event& e) {
 *     if (e.type == EventType::UserDefined &&
 *         e.userType == MY_NETWORK_EVENT) {
 *
 *         auto* netEvent = static_cast<NetworkEvent*>(e.userData);
 *         LOG_INFO("Player {} connected at {} (ping: {}ms)",
 *             netEvent->playerName, netEvent->position, netEvent->ping);
 *
 *         spawnPlayer(netEvent->playerName, netEvent->position);
 *     }
 * }
 *
 * // === KEYBOARD MODIFIERS ===
 * void Game::onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed) {
 *         // Quick save (Ctrl+S)
 *         if (e.keyboard.key == SDLK_S && e.keyboard.ctrl) {
 *             quickSave();
 *         }
 *
 *         // Quick load (Ctrl+L)
 *         if (e.keyboard.key == SDLK_L && e.keyboard.ctrl) {
 *             quickLoad();
 *         }
 *
 *         // Screenshot (Ctrl+Shift+P)
 *         if (e.keyboard.key == SDLK_P &&
 *             e.keyboard.ctrl && e.keyboard.shift) {
 *             takeScreenshot();
 *         }
 *     }
 * }
 *
 * // === HANDLED FLAG (PRIORITY) ===
 * // UI checks first
 * void UI::onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed && !e.handled) {
 *         if (e.keyboard.key == SDLK_ESCAPE) {
 *             closeMenu();
 *             e.handled = true;  // Consumed, stop propagation
 *         }
 *     }
 * }
 *
 * // Gameplay checks second (won't see handled events)
 * void Player::onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed && !e.handled) {
 *         if (e.keyboard.key == SDLK_ESCAPE) {
 *             // Won't trigger - UI handled it
 *         }
 *     }
 * }
 * ```
 *
 * CORE EVENT TYPES - Performance-Critical:
 *
 * Window events (lifecycle):
 * - WindowClose: User clicked X or Alt+F4 (shutdown)
 * - WindowResize: Dimensions changed (update viewport, aspect ratio)
 * - WindowFocus: Gained focus (resume, show cursor)
 * - WindowLostFocus: Lost focus (pause, release keys)
 *
 * Keyboard events (input):
 * - KeyPressed: Key pressed (includes repeat flag)
 * - KeyReleased: Key released
 * - Data: key code, scancode, repeat, shift/ctrl/alt modifiers
 *
 * Mouse events (camera, UI):
 * - MouseButtonPressed: Mouse button down (shooting, UI click)
 * - MouseButtonReleased: Mouse button up
 * - MouseMoved: Cursor moved (camera rotation, UI hover)
 * - MouseScrolled: Wheel scrolled (zoom, inventory scroll)
 * - Data: button, position (x, y), delta (dx, dy)
 *
 * USER EVENT TYPES - Game-Specific:
 *
 * Examples (define your own):
 * - NetworkEvent: Player joined, chat message, server update
 * - AIEvent: Enemy spotted, patrol changed, alert triggered
 * - QuestEvent: Mission complete, objective updated, reward
 * - AudioEvent: Sound finished, music transition
 * - PhysicsEvent: Collision detected, trigger entered
 * - GameStateEvent: Level loaded, checkpoint, game over
 *
 * UNION-BASED STORAGE - How It Works:
 *
 * ```cpp
 * union {
 *     struct { int width, height; } size;        // WindowResize
 *     struct { int key; bool shift; } keyboard;  // KeyPressed
 *     struct { float x, y; } mouseMove;          // MouseMoved
 *     // ... (all core events)
 * };
 * ```
 *
 * Memory layout:
 * - All structs share same memory (only one active)
 * - Size = largest struct (not sum of all)
 * - Example: sizeof(Event) = ~64 bytes (not 256+ bytes)
 *
 * Benefits:
 * - Stack-allocated (no heap, no malloc)
 * - Cache-friendly (small, contiguous)
 * - Zero allocation overhead (critical for thousands per frame)
 *
 * Trade-off:
 * - Harder to add new core events (must update union)
 * - Worth it: Performance > convenience for core events
 *
 * KEYBOARD MODIFIERS - Common Use Cases:
 *
 * ```cpp
 * if (e.type == EventType::KeyPressed) {
 *     // Save (Ctrl+S)
 *     if (e.keyboard.key == SDLK_S && e.keyboard.ctrl) {
 *         saveGame();
 *     }
 *
 *     // Undo (Ctrl+Z)
 *     if (e.keyboard.key == SDLK_Z && e.keyboard.ctrl) {
 *         undo();
 *     }
 *
 *     // Redo (Ctrl+Shift+Z)
 *     if (e.keyboard.key == SDLK_Z &&
 *         e.keyboard.ctrl && e.keyboard.shift) {
 *         redo();
 *     }
 *
 *     // Sprint (Shift+W)
 *     if (e.keyboard.key == SDLK_W && e.keyboard.shift) {
 *         sprint();
 *     }
 * }
 * ```
 *
 * MOUSE DELTA - Camera Control:
 *
 * Normal mode (window):
 * - delta = current position - previous position
 * - Affected by cursor speed, acceleration
 *
 * Relative mode (locked cursor, FPS):
 * - delta = raw hardware movement
 * - Unaffected by cursor settings (better raw input)
 * - Use for FPS cameras (more responsive)
 *
 * ```cpp
 * if (e.type == EventType::MouseMoved) {
 *     // Use delta for camera rotation
 *     float yaw = e.mouseMove.deltaX * sensitivity;
 *     float pitch = e.mouseMove.deltaY * sensitivity;
 *     camera.rotate(yaw, pitch);
 * }
 * ```
 *
 * HANDLED FLAG - Event Priority:
 *
 * Priority chain:
 * 1. UI (highest priority, checks first)
 * 2. Debug overlay (developer tools)
 * 3. Gameplay (player, game logic)
 * 4. Background systems (analytics, recording)
 *
 * ```cpp
 * // UI layer (priority 1)
 * if (!e.handled && e.type == EventType::KeyPressed) {
 *     if (e.keyboard.key == SDLK_TAB) {
 *         toggleInventory();
 *         e.handled = true;  // Consumed
 *     }
 * }
 *
 * // Gameplay layer (priority 3, won't see handled events)
 * if (!e.handled && e.type == EventType::KeyPressed) {
 *     if (e.keyboard.key == SDLK_TAB) {
 *         // Won't trigger - UI consumed it
 *     }
 * }
 * ```
 *
 * MEMORY MANAGEMENT - Critical:
 *
 * Core events (union-based):
 * - Stack-allocated (automatic cleanup)
 * - No manual memory management needed
 *
 * User events (pointer-based):
 * - Heap-allocated (manual cleanup REQUIRED)
 * - Caller must delete after handling
 *
 * ```cpp
 * // Create user event
 * Event e;
 * e.userData = new MyEvent{...};
 *
 * // Send event
 * application.onEvent(e);
 *
 * // Clean up (REQUIRED!)
 * delete static_cast<MyEvent*>(e.userData);
 * ```
 *
 * Best practice: Use smart pointers
 * ```cpp
 * e.userData = new std::shared_ptr<MyEvent>(...);
 * // Automatic cleanup when last reference dies
 * ```
 *
 * CURRENT STATE (September 23, 2025):
 * - Union-based core events (window, keyboard, mouse)
 * - Pointer-based user events (game-specific)
 * - Handled flag (priority system)
 * - SDL translation layer (Window::translateSDLEvent)
 * - Keyboard modifiers (shift, ctrl, alt)
 * - Mouse delta (normal + relative mode)
 * - Zero-initialization (union safety)
 * - Status: Production-ready, performance-optimized
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. No Event Queue:
 * - Events processed immediately (no deferred processing)
 * - Future: Event queue with priority
 *
 * 2. No Event Filtering:
 * - All events delivered to all handlers
 * - Future: Subscribe to specific event types 
 *
 * 3. Manual Memory Management:
 * - User events require manual delete
 * - Future: Smart pointer support
 *
 * 4. Hard to Add Core Events:
 * - Must update union (recompile engine)
 * - Trade-off: Performance > convenience
 *
 * 5. No Event Recording:
 * - Can't record/playback events (replays, testing)
 * - Future: Event recording system 
 *
 * 6. No Event Pooling:
 * - User events always allocate (no reuse)
 * - Future: Object pooling 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 23, 2025: Initial implementation
 * - Union-based core events (performance)
 * - Pointer-based user events (flexibility)
 * - SDL translation layer
 * - Handled flag (priority)
 * - Status: Production-ready
 *
 * (Smart Pointers):
 * - std::shared_ptr support for user events
 * - Automatic cleanup (no manual delete)
 * - Time: 1-2 days
 *
 * (Event Queue):
 * - Deferred event processing
 * - Priority-based ordering
 * - Time: 3-5 days
 *
 * (Event Filtering):
 * - Subscribe to specific event types
 * - Reduce unnecessary handler calls
 * - Time: 2-3 days
 *
 * (Event Recording):
 * - Record events for replays
 * - Playback for automated testing
 * - Time: 1 week
 *
 * (Event Pooling):
 * - Object pooling for user events
 * - Reduce allocation overhead
 * - Time: 2-3 days
 *
 * (Additional Events):
 * - Gamepad events (buttons, axes, connection)
 * - Touch events (mobile, gestures)
 * - Drop events (drag-and-drop files)
 * - Time: 1-2 weeks
 *
 * DEPENDENCIES:
 * - None (self-contained header)
 *
 * THREAD SAFETY:
 * - Events not thread-safe (no synchronization)
 * - All event handling on main thread (SDL requirement)
 *
 * REFERENCES:
 * - Casey Muratori's Handmade Hero: Union-based event design
 * - SDL3 documentation: Event handling
 * - Game Engine Architecture 3rd Ed.: Event systems
 *
 * HISTORY:
 * September 23, 2025: Initial implementation
 * - Union-based design for core events (performance)
 * - Pointer-based design for user events (flexibility)
 * - Inspired by Casey Muratori's Handmade Hero
 * - SDL translation layer (Window::translateSDLEvent)
 * - Handled flag for event priority
 * - Keyboard modifiers (shift, ctrl, alt)
 * - Mouse delta (normal + relative mode)
 * - Zero-initialization constructor (union safety)
 * - Trade-off accepted: Harder to add core events, but performance critical
 * - Result: Fast, cache-friendly event system
 *
 */

namespace Engine
{
	enum class EventType
	{
		None = 0,

		// Core engine events (0-999)
		WindowClose, WindowResize, WindowFocus, WindowLostFocus,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,

		// Reserved for future engine events (100-999)

		// User-defined events (1000+)
		UserDefined = 1000
	};

	struct Event
	{
		EventType type = EventType::None;
		bool handled = false;

		// For user-defined events
		int userType = 0;
		void* userData = nullptr;

		// Constructor to initialize the union which is on the stack to zero the shared memory, that way there is no garbage values = no undefined behavior
		Event() : keyboard{} {}

		// Core event data (union for performance)
		union
		{
			// Window resize
			struct
			{
				int width, height;
			} size;

			// Key press/release
			struct
			{
				int key; // Key code for SDL
				int scancode; // Hardware scancode
				bool repeat; // Are we holding the key?
				bool shift, ctrl, alt; // Common modifier keys
			} keyboard;

			// Mouse move
			struct
			{
				float x, y; // Current position
				float deltaX, deltaY; // Movement since last frame
			} mouseMove;

			// Mouse button press/release
			struct
			{
				int button; // 0 = left, 1 = right, 2 = middle
				float x, y; // Click position
			} mouseButton;

			// Mouse scrolled
			struct
			{
				float deltaX, deltaY; // deltaX = 0, just deltaY up and down scroll
			} scroll;

			// Windows focus and lost focus dont need 
		};
	};
}