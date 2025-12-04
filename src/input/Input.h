#pragma once
#include "math/EngineMath.h"
#include <SDL3/SDL_keycode.h>
#include <array>
#include <unordered_map>
#include <string>

/*
 * Input.h
 *
 * PURPOSE:
 * Game-friendly input abstraction providing continuous state queries and action mapping.
 * Bridges raw SDL events with gameplay code through intuitive polling API. Handles complex
 * input patterns (double-tap, hold duration, consumption, rebinding). Essential for
 * responsive gameplay and flexible control schemes.
 *
 * DESIGN RATIONALE (September 25-26, 2025):
 * Problem: Raw SDL events temporal ("key pressed THIS frame"). Games need continuous state
 * ("key IS pressed"). Need to prevent double-handling (UI consumes, gameplay doesn't see).
 * Need rebindable controls (user settings). Need advanced patterns (double-tap, charge
 * attacks). SDL keycodes sparse (0-1 billion, wastes memory).
 *
 * Solution: Comprehensive input system with persistent state and smart features.
 * - Persistent state: "IS key pressed" not "WAS key pressed this frame"
 * - Consumption flags: First caller gets it (prevents double-handling)
 * - Action mapping: Rebindable controls ("Jump" -> SDLK_SPACE)
 * - Timing queries: Hold duration, double-tap, triple-tap
 * - Compact storage: Remap sparse SDL keycodes to dense array (256 slots)
 * - Result: Feature-complete input system ready for any gameplay scenario
 *
 * Key Insight: Input fundamental to gameplay (movement, combat, UI). Over-engineered by
 * design (intentional). Need comprehensive features upfront (not incremental). State-based
 * queries natural for games ("IS pressed" reads better than event callbacks). Consumption
 * pattern essential (UI priority over gameplay). Action mapping enables player customization.
 *
 * DESIGN PHILOSOPHY:
 * - State-based: Continuous queries ("IS pressed")
 * - Frame lifecycle: Reset per-frame flags, update timing
 * - Consumption: Prevent double-handling (UI > gameplay)
 * - Action mapping: Rebindable controls (player settings)
 * - Timing queries: Advanced patterns (double-tap, charge)
 * - Memory efficient: Compact keycode remapping (14KB total)
 *
 * KEY CONCEPTS:
 * 1. State vs Events:
 *    - Events: Temporal ("key pressed THIS FRAME", one-time)
 *    - State: Continuous ("key IS pressed", persistent)
 *    - Games need state (movement, combat, camera)
 *
 * 2. Consumption Pattern:
 *    - First caller can consume input (consume=true)
 *    - Prevents double-handling (UI > gameplay priority)
 *    - Critical for layered systems (modal UI, overlays)
 *
 * 3. Action Mapping:
 *    - Named actions ("Jump", "Shoot", "Interact")
 *    - Rebindable at runtime (user settings)
 *    - Save/load from file (persistent config)
 *
 * 4. Frame Lifecycle:
 *    - beginFrame(): Reset per-frame flags, decay taps
 *    - onEvent(): Update state from SDL events
 *    - Game loop: Query input state
 *    - endFrame(): Update timing (held duration)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION ===
 * Input::init();
 *
 * // Setup rebindable controls
 * Input::bindAction("Jump", SDLK_SPACE);
 * Input::bindAction("Shoot", SDLK_MOUSE_LEFT);
 * Input::bindAction("Interact", SDLK_E);
 *
 * // === FRAME LOOP ===
 * void Application::run() {
 *     while (running) {
 *         Input::beginFrame();  // Reset per-frame flags
 *
 *         // Process SDL events
 *         SDL_Event e;
 *         while (SDL_PollEvent(&e)) {
 *             Event event = convertSDLEvent(e);
 *             Input::onEvent(event);  // Update input state
 *         }
 *
 *         update(deltaTime);  // Game queries input
 *         render();
 *
 *         Input::endFrame();  // Update timing
 *     }
 * }
 *
 * // === BASIC MOVEMENT ===
 * void Player::update(float deltaTime) {
 *     // Continuous state queries (held keys)
 *     if (Input::isKeyPressed(SDLK_W)) moveForward(deltaTime);
 *     if (Input::isKeyPressed(SDLK_S)) moveBackward(deltaTime);
 *     if (Input::isKeyPressed(SDLK_A)) strafeLeft(deltaTime);
 *     if (Input::isKeyPressed(SDLK_D)) strafeRight(deltaTime);
 * }
 *
 * // === UI - CONSUMED INPUT ===
 * void UI::update() {
 *     // UI checks first, consumes
 *     if (Input::isKeyJustPressed(SDLK_TAB, true)) {
 *         toggleScoreboard();  // Consumed
 *     }
 *
 *     if (Input::isKeyJustPressed(SDLK_ESCAPE, true)) {
 *         closeMenu();  // Consumed
 *     }
 * }
 *
 * void Player::update() {
 *     // Gameplay checks second, doesn't see consumed input
 *     if (Input::isKeyJustPressed(SDLK_TAB, true)) {
 *         // Won't trigger - UI consumed it
 *     }
 * }
 *
 * // === CHARGE ATTACK (TIMING) ===
 * void Player::updateCombat() {
 *     if (Input::isKeyPressed(SDLK_SPACE)) {
 *         // Get hold duration
 *         float chargeTime = Input::getKeyHeldDuration(SDLK_SPACE);
 *         updateChargeAttack(chargeTime);  // 0-3 seconds
 *     }
 *
 *     if (Input::isKeyJustReleased(SDLK_SPACE, true)) {
 *         float chargeTime = Input::getTimeSinceKeyPressed(SDLK_SPACE);
 *         releaseChargeAttack(chargeTime);  // Damage scales with charge
 *     }
 * }
 *
 * // === DODGE (DOUBLE-TAP) ===
 * void Player::updateDodge() {
 *     // Double-tap within 300ms
 *     if (Input::wasKeyDoubleTapped(SDLK_D)) dodgeRight();
 *     if (Input::wasKeyDoubleTapped(SDLK_A)) dodgeLeft();
 *     if (Input::wasKeyDoubleTapped(SDLK_W)) dodgeForward();
 *     if (Input::wasKeyDoubleTapped(SDLK_S)) dodgeBackward();
 * }
 *
 * // === ACTION MAPPING (REBINDABLE) ===
 * void Player::update() {
 *     // Use action names (not hardcoded keys)
 *     if (Input::isActionJustPressed("Jump")) jump();
 *     if (Input::isActionPressed("Shoot")) weapon.fire();
 *     if (Input::isActionJustPressed("Interact")) interact();
 * }
 *
 * // Settings menu allows rebinding
 * void SettingsMenu::rebindControl(const std::string& action) {
 *     showPrompt("Press new key for " + action);
 *
 *     // Wait for key press
 *     keyCode newKey = Input::getLastKeyPressed();
 *     Input::bindAction(action, newKey);
 *     Input::saveBindings("config/controls.json");
 * }
 *
 * // === MOUSE CAMERA ===
 * void Camera::update() {
 *     // Mouse delta for smooth rotation
 *     vec2 mouseDelta = Input::getMouseDelta();
 *     rotate(mouseDelta.x * sensitivity, mouseDelta.y * sensitivity);
 *
 *     // Mouse wheel for zoom
 *     float wheelDelta = Input::getMouseWheelDelta();
 *     adjustZoom(wheelDelta);
 * }
 *
 * // === MOUSE BUTTONS ===
 * void Player::updateWeapon() {
 *     if (Input::isMouseButtonPressed(MouseButton::Left)) {
 *         primaryFire();  // Continuous (machine gun)
 *     }
 *
 *     if (Input::isMouseButtonJustPressed(MouseButton::Right, true)) {
 *         secondaryFire();  // One-shot (grenade)
 *     }
 * }
 * ```
 *
 * STATE VS EVENTS - Why Both?
 *
 * Event system (temporal, one-time):
 * ```cpp
 * void onEvent(Event& e) {
 *     if (e.type == EventType::KeyPressed && e.key == SDLK_SPACE) {
 *         // Fires ONCE when key pressed
 *         oneTimeAction();
 *     }
 * }
 * ```
 * Good for: System-level handling (window close, key repeat)
 *
 * Input system (continuous, persistent):
 * ```cpp
 * void update(float deltaTime) {
 *     if (Input::isKeyPressed(SDLK_SPACE)) {
 *         // Checks EVERY FRAME while held
 *         continuousMovement(deltaTime);
 *     }
 * }
 * ```
 * Good for: Movement, shooting, camera control (gameplay)
 *
 * CONSUMPTION PATTERN - Preventing Double-Handling:
 *
 * Problem without consumption:
 * ```cpp
 * // UI checks
 * if (Input::isKeyJustPressed(SDLK_E)) {
 *     openInventory();  // Opens inventory
 * }
 *
 * // Gameplay checks (SAME FRAME)
 * if (Input::isKeyJustPressed(SDLK_E)) {
 *     interact();  // Also triggers! (BUG)
 * }
 * // Result: Opens inventory AND interacts (unintended)
 * ```
 *
 * Solution with consumption:
 * ```cpp
 * // UI checks first, consumes
 * if (Input::isKeyJustPressed(SDLK_E, true)) {
 *     openInventory();  // Opens inventory, consumes input
 * }
 *
 * // Gameplay checks second
 * if (Input::isKeyJustPressed(SDLK_E, true)) {
 *     interact();  // Doesn't trigger (input consumed)
 * }
 * // Result: Opens inventory only (correct)
 * ```
 *
 * COMPACT KEY MAPPING - Memory Efficiency:
 *
 * Problem: SDL keycodes sparse (0 to ~1 billion)
 * - Direct indexing: bool keyStates[1 billion] = 1GB memory (wasteful!)
 *
 * Solution: Remap to dense array (256 slots)
 * ```cpp
 * int remapKey(keyCode key) {
 *     // ASCII keys (A-Z, 0-9): Direct mapping (0-127)
 *     if (key >= 0 && key < 128) {
 *         return key;
 *     }
 *
 *     // Special keys: Compact to 128-255
 *     switch (key) {
 *         case SDLK_SPACE: return 128;
 *         case SDLK_ESCAPE: return 129;
 *         case SDLK_RETURN: return 130;
 *         // ... (total 128 special keys)
 *     }
 *
 *     return 0;  // Unknown key
 * }
 * ```
 * Result: 256 keys x 56 bytes = 14KB (99.999% memory savings!)
 *
 * FRAME LIFECYCLE - When Things Happen:
 *
 * ```cpp
 * void Application::run() {
 *     while (running) {
 *         // 1. RESET PER-FRAME FLAGS
 *         Input::beginFrame();
 *         // - pressedThisFrame = false
 *         // - releasedThisFrame = false
 *         // - consumedPress = false
 *         // - consumedRelease = false
 *         // - Decay tap counts (500ms timeout)
 *
 *         // 2. PROCESS SDL EVENTS
 *         SDL_Event e;
 *         while (SDL_PollEvent(&e)) {
 *             Event event = convertSDLEvent(e);
 *             Input::onEvent(event);
 *             // - Update pressed/released flags
 *             // - Record timestamps
 *             // - Increment tap counts
 *         }
 *
 *         // 3. GAME LOGIC (QUERY INPUT)
 *         update(deltaTime);
 *         // - isKeyPressed() checks
 *         // - isKeyJustPressed() checks (may consume)
 *         // - getKeyHeldDuration() queries
 *
 *         // 4. RENDER
 *         render();
 *
 *         // 5. UPDATE TIMING
 *         Input::endFrame();
 *         // - Update held durations
 *         // - Update time since press/release
 *     }
 * }
 * ```
 *
 * TIMING QUERIES - Advanced Patterns:
 *
 * Hold duration (charge attacks):
 * ```cpp
 * if (Input::isKeyPressed(SDLK_SPACE)) {
 *     float chargeTime = Input::getKeyHeldDuration(SDLK_SPACE);
 *     // 0.0s -> 3.0s (max charge)
 *     updateChargeVisual(chargeTime / 3.0f);  // 0-100% visual
 * }
 * ```
 *
 * Double-tap (dodge):
 * ```cpp
 * if (Input::wasKeyDoubleTapped(SDLK_D, 0.3f)) {
 *     // Second press within 300ms
 *     dodgeRight();
 * }
 * ```
 *
 * Triple-tap (special move):
 * ```cpp
 * if (Input::wasKeyTripleTapped(SDLK_SPACE, 0.5f)) {
 *     // Third press within 500ms
 *     specialMove();
 * }
 * ```
 *
 * Time since release (buffer window):
 * ```cpp
 * // Jump buffer (jumped just before landing)
 * if (Input::getTimeSinceKeyPressed(SDLK_SPACE) < 0.1f && isGrounded) {
 *     jump();  // Pressed up to 100ms before landing
 * }
 * ```
 *
 * ACTION MAPPING - Rebindable Controls:
 *
 * Setup (config file or defaults):
 * ```cpp
 * void loadControls() {
 *     Input::bindAction("Jump", SDLK_SPACE);
 *     Input::bindAction("Shoot", SDLK_MOUSE_LEFT);
 *     Input::bindAction("Interact", SDLK_E);
 *     Input::bindAction("Sprint", SDLK_LSHIFT);
 *     Input::bindAction("Crouch", SDLK_LCTRL);
 * }
 * ```
 *
 * Gameplay (action names, not keys):
 * ```cpp
 * void Player::update() {
 *     // Use action names (rebindable)
 *     if (Input::isActionPressed("Sprint")) {
 *         speed = sprintSpeed;
 *     }
 *
 *     if (Input::isActionJustPressed("Jump")) {
 *         jump();
 *     }
 *
 *     if (Input::isActionPressed("Shoot")) {
 *         weapon.fire();
 *     }
 * }
 * ```
 *
 * Rebind (settings menu):
 * ```cpp
 * void rebindAction(const std::string& action) {
 *     showPrompt("Press new key for " + action);
 *
 *     // Wait for key press
 *     keyCode newKey = Input::getLastKeyPressed();
 *
 *     // Update binding
 *     Input::bindAction(action, newKey);
 *
 *     // Save to file
 *     Input::saveBindings("config/controls.json");
 * }
 * ```
 *
 * MEMORY FOOTPRINT:
 *
 * Per-key state (KeyState):
 * - bool pressed: 1 byte
 * - bool pressedThisFrame: 1 byte
 * - bool releasedThisFrame: 1 byte
 * - bool consumedPress: 1 byte
 * - bool consumedRelease: 1 byte
 * - float timePressed: 4 bytes
 * - float timeReleased: 4 bytes
 * - float heldDuration: 4 bytes
 * - int tapCount: 4 bytes
 * - float lastTapTime: 4 bytes
 * - int pressCountThisFrame: 4 bytes
 * - int releaseCountThisFrame: 4 bytes
 * - Total: 32 bytes (with padding: 56 bytes)
 *
 * Total memory:
 * - 256 keys x 56 bytes = 14 KB (keyboard)
 * - 8 mouse buttons x 32 bytes = 256 bytes (mouse)
 * - Action maps: ~1 KB (typical)
 * - Total: ~15 KB (negligible)
 *
 * CURRENT STATE (September 25-26, 2025):
 * - Keyboard state queries (pressed, just pressed, just released)
 * - Mouse state queries (buttons, position, delta, wheel)
 * - Consumption pattern (prevent double-handling)
 * - Timing queries (hold duration, time since press/release)
 * - Multi-tap detection (double-tap, triple-tap, custom window)
 * - Action mapping (rebindable controls, save/load)
 * - Compact storage (256 keys, 14KB memory)
 * - Frame lifecycle (beginFrame, onEvent, endFrame)
 * - Status: Production-ready, comprehensive
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Keyboard + Mouse Only:
 * - No gamepad support
 * - Future: Xbox/PlayStation/Switch controllers 
 *
 * 2. Single Input Context:
 * - No context switching (menu vs gameplay)
 * - Future: Input contexts with automatic rebinding 
 *
 * 3. No Input Recording:
 * - Can't record/playback input (replays, testing)
 * - Future: Input recording system 
 *
 * 4. No Gesture Recognition:
 * - No swipe, pinch, rotate (mobile)
 * - Future: Touch input support 
 *
 * 5. No Combo System:
 * - No sequence detection (fighting games)
 * - Future: Combo system 
 *
 * 6. No Chord Detection:
 * - No modifier combinations (Ctrl+Shift+Key)
 * - Future: Chord detection 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 25-26, 2025: Initial implementation
 * - Comprehensive input system (keyboard + mouse)
 * - State queries, consumption, timing, multi-tap
 * - Action mapping, rebinding, save/load
 * - Intentionally over-engineered (input is critical)
 * - Status: Production-ready
 *
 * (Gamepad Support):
 * - Xbox, PlayStation, Switch controllers
 * - Button/axis queries, deadzone handling, rumble
 * - Time: 1 week
 *
 * (Input Contexts):
 * - Context switching (menu, gameplay, vehicle, cutscene)
 * - Automatic rebinding per context
 * - Time: 3-5 days
 *
 * (Recording/Playback):
 * - Input recording for replays
 * - Playback for automated testing
 * - Time: 1 week
 *
 * (Touch Input):
 * - Multi-touch, swipe, pinch, rotate (mobile)
 * - Gesture recognition
 * - Time: 2 weeks
 *
 * (Combo System):
 * - Sequence detection (fighting games)
 * - Time window, canceling, priority
 * - Time: 1 week
 *
 * DEPENDENCIES:
 * - math/EngineMath.h: vec2 (mouse delta)
 * - <SDL3/SDL_keycode.h>: SDL keycode definitions
 * - <array>: std::array (key/button state storage)
 * - <unordered_map>: Action mapping (name -> keycode)
 * - <string>: Action names
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Static members (no mutex)
 * - All operations on main thread only (event loop)
 *
 * REFERENCES:
 * - SDL3 documentation: Input handling
 * - Game Programming Patterns: Input system architecture
 *
 * HISTORY:
 * September 25-26, 2025: Initial implementation
 * - Comprehensive input system (keyboard + mouse)
 * - State queries (pressed, just pressed, just released)
 * - Consumption pattern (prevent double-handling)
 * - Timing queries (hold duration, time since press/release)
 * - Multi-tap detection (double-tap, triple-tap, custom window)
 * - Action mapping (rebindable controls, save/load)
 * - Compact key storage (sparse SDL -> dense array, 14KB)
 * - Frame lifecycle (beginFrame, onEvent, endFrame)
 * - Mouse support (buttons, position, delta, wheel)
 * - Intentionally over-engineered (input is fundamental)
 * - Result: Production-ready, feature-complete input system
 *
 */

namespace Engine
{
	using keyCode = SDL_Keycode;

	enum class MouseButton
	{
		Left = 0, Right = 1, Middle = 2, 
		Button4 = 3, Button5 = 4 // Side mouse buttons
	};

	class Input
	{
	public:
		// Lifecycle
		static void init();
		static void shutdown();
		static void beginFrame();
		static void endFrame();
		// Maybe include the event header in this header instead of cpp
		static void onEvent(struct Event& e);

		// Core API - Keyboard

		// Continous state queries for movement and held actions
		static bool isKeyPressed(keyCode key);
		static bool isKeyUp(keyCode key) { return !isKeyPressed(key); }

		// Frame specific events toggles and one time actions
		// consume = true; First caller gets it to prevent double-handling
		// consume = false; Multiple systems can check
		static bool isKeyJustPressed(keyCode key, bool consume = true);
		static bool isKeyJustReleased(keyCode key, bool consume = true);

		// Core API - Mouse

		// Button states
		static bool isMouseButtonPressed(MouseButton button);
		static bool isMouseButtonJustPressed(MouseButton button, bool consume = true);
		static bool isMouseButtonJustReleased(MouseButton button, bool consume = true);

		// Position and movement
		static Engine::vec2 getMousePosition() { return { s_mouseX, s_mouseY }; }
		static Engine::vec2 getMouseDelta() { return { s_mouseDeltaX, s_mouseDeltaY }; }
		static void setMousePosition(float x, float y); 

		// Timing

		// For held actions (charging attacks, press-and-hold interactions)
		static float getKeyHeldDuration(keyCode key);
		static float getTimeSinceKeyPressed(keyCode key);
		static float getTimeSinceKeyReleased(keyCode key);

		// Multi-tap detection (dodge rolls, double jumps)
		static bool wasKeyDoubleTapped(keyCode key, float windowSeconds = 0.3f);
		static bool wasKeyTripleTapped(keyCode key, float windowSeconds = 0.5f);
		static int getKeyTapCount(keyCode key, float windowSeconds = 1.0f);

		// Frame Analysis
		
		// For detecting state changes and macro usage
		static bool hasKeyStateChangedThisFrame(keyCode key);
		static int getKeyPressCountThisFrame(keyCode key); // Anti-cheat????

		// Mouse Special
		static float getMouseWheelDelta() { return s_mouseWheelDelta; }
		static void setMouseCursorVisible(bool visible);
		static void setMouseCursorLocked(bool locked); // FPS mouse capture
		static bool isMouseInWindow() { return s_mouseInWindow; }

		// Action Mapping (Rebindable Controls)

		static void bindAction(const std::string& actionName, keyCode key);
		static void bindAction(const std::string& actionName, MouseButton button);
		static void unbindAction(const std::string& actionName);
		static bool isActionPressed(const std::string& actionName);
		static bool isActionJustPressed(const std::string& actionName, bool consume);

		// Able to save user settings
		static void saveBindings(const std::string& filename);
		static void loadBindings(const std::string& filename);

		// Query for UI/Debug

		static std::string getKeyName(keyCode key); // "space", "x"
		static keyCode getLastKeyPressed() { return s_lastKeyPressed; } // For rebinding

	private:
		// Per-key state tracking
		struct KeyState
		{
			// Current state
			bool pressed = false;

			// Frame-specific flags
			bool pressedThisFrame = false;
			bool releasedThisFrame = false;
			bool consumedPress = false;
			bool consumedRelease = false;

			// Timing information
			float timePressed = 0.0f;
			float timeReleased = 0.0f;
			float heldDuration = 0.0f;

			// Multi-tap tracking
			int tapCount = 0;
			float lastTapTime = 0.0f;

			// Frame edge cases
			int pressCountThisFrame = 0;
			int releaseCountThisFrame = 0;
		};

		struct MouseButtonState
		{
			bool pressed = false;
			bool pressedThisFrame = false;
			bool releasedThisFrame = false;
			bool consumedPress = false;
			bool consumedRelease = false;
			float timePressed = 0.0f;
			float timeReleased = 0.0f;
			float heldDuration = 0.0f;
		};

		// State storage (256 keys with remapping, 8 mouse buttons)
		static std::array<KeyState, 256> s_keyStates;
		static std::array<MouseButtonState, 8> s_mouseStates;

		// Mouse Tracking
		static float s_mouseX, s_mouseY;
		static float s_mouseDeltaX, s_mouseDeltaY;
		static float s_mouseWheelDelta;
		static bool s_mouseInWindow;

		// Action mapping tables
		static std::unordered_map<std::string, keyCode> s_keyActions;
		static std::unordered_map<std::string, keyCode> s_mouseActions;

		// Utility
		static keyCode s_lastKeyPressed;

		// Sparse SDL keyCode -> Dense array index (0-255)
		static int remapKey(keyCode key);

		// Internal timing updates
		static void updateKeyTiming(float deltaTime);
		static void updateMouseTiming(float deltaTime);
	};
}