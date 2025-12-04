#include "input/Input.h"
#include "events/Event.h"
#include "core/Logger.h"
#include "core/EngineTime.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>

/*
 * Input.cpp - Input System Implementation
 *
 * PURPOSE:
 * Implements comprehensive input system with persistent state queries, consumption pattern,
 * timing queries, multi-tap detection, and action mapping. Bridges SDL events with gameplay
 * code through intuitive polling API.
 *
 * IMPLEMENTATION NOTES (September 25-26, 2025):
 *
 * KEY DESIGN DECISIONS:
 *
 * 1. Static Design:
 * - Global access (no passing Input& everywhere)
 * - Games need input everywhere (player, UI, debug)
 * - Matches Time system architecture
 *
 * 2. Memory Layout:
 * - Fixed arrays (not maps) for performance
 * - 256 key slots (all common keys + room to grow)
 * - No runtime allocations after init
 * - Total: ~15KB (negligible)
 *
 * 3. Frame Lifecycle:
 * - beginFrame(): Reset per-frame flags, decay tap counts
 * - onEvent(): Update state from SDL events
 * - Game loop: Query input state
 * - endFrame(): Update timing (held duration)
 * - Ensures consistent state throughout frame
 *
 * 4. Consumption Logic:
 * - First caller wins (prevents double-handling)
 * - UI processes before gameplay (priority)
 * - Non-consumed queries available (hints, tooltips)
 *
 * 5. Tap Timing:
 * - Tap counts decay after 500ms inactivity (prevents stale data)
 * - Double-tap: 300ms window between presses
 * - All timing uses EngineTime (frame-rate independent)
 *
 * 6. Key Remapping:
 * - SDL keycodes sparse (0-1 billion, wastes memory)
 * - Remap to dense array (256 slots, saves ~1GB)
 * - ASCII keys (0-127): Direct mapping
 * - Special keys (128-255): Compacted
 *
 * CRITICAL IMPLEMENTATION DETAILS:
 *
 * State Updates:
 * - Only update on actual state change (prevents spurious events)
 * - Reset deltas in beginFrame() (prevents accumulation)
 * - Clear all keys on window focus loss (prevents stuck keys)
 *
 * Mouse Handling:
 * - Relative mode: Use delta from SDL directly
 * - Normal mode: Calculate delta from position
 * - Reset delta on setMousePosition() (prevents jump)
 *
 * Tap Detection:
 * - Increment tapCount if press within 300ms
 * - Reset to 1 if too slow (new sequence)
 * - Decay in beginFrame() after 500ms (cleanup)
 *
 * Action Mapping:
 * - Check keyboard bindings first
 * - Then check mouse bindings
 * - Return true on first match
 * - Save/load uses simple CSV format
 *
 * REFERENCES:
 * - See Input.h for full API documentation
 * - SDL3 documentation for event handling
 */

namespace Engine
{
	// static member definitions
	std::array<Input::KeyState, 256> Input::s_keyStates;
	std::array<Input::MouseButtonState, 8> Input::s_mouseStates;
	float Input::s_mouseX = 0.0f;
	float Input::s_mouseY = 0.0f;
	float Input::s_mouseDeltaX = 0.0f;
	float Input::s_mouseDeltaY = 0.0f;
	float Input::s_mouseWheelDelta = 0.0f;
	bool Input::s_mouseInWindow = true;
	keyCode Input::s_lastKeyPressed = SDLK_UNKNOWN;
	std::unordered_map<std::string, keyCode> Input::s_keyActions;
	std::unordered_map<std::string, keyCode> Input::s_mouseActions;

	void Input::init()
	{
		LOG_INFO("Input system initialized");

		// Clear all states to known values
		for (auto& state : s_keyStates)
		{
			state = KeyState{};
		}
		for (auto& state : s_mouseStates)
		{
			state = MouseButtonState{};
		}

		// Todo: Load default bindings from config file
		// loadBindings ("assets/config/default_controls.ini")
	}

	void Input::shutdown()
	{
		// Todo: Save current bindings if modified
		// if (s_bindingsModified) saveBindings("user_controls.ini")

		LOG_INFO("Input system shut down");
	}

	void Input::beginFrame()
	{
		// Reset all per-frame flags
		// This ensures "just pressed" is only true for one frame

		for (auto& key : s_keyStates) {
			key.pressedThisFrame = false;
			key.releasedThisFrame = false;
			key.consumedPress = false;
			key.consumedRelease = false;
			key.pressCountThisFrame = 0;
			key.releaseCountThisFrame = 0;
		
            // Decay tap count if no recent activity
            float timeSinceLastTap = EngineTime::getTime() - key.lastTapTime;
            if (timeSinceLastTap > 0.5f)
            {
                key.tapCount = 0;
            }
        }

		for (auto& mb : s_mouseStates) {
			mb.pressedThisFrame = false;
			mb.releasedThisFrame = false;
			mb.consumedPress = false;
			mb.consumedRelease = false;
		}

		// Reset deltas
		s_mouseDeltaX = 0.0f;
		s_mouseDeltaY = 0.0f;
		s_mouseWheelDelta = 0.0f;
	}

	void Input::endFrame()
	{
		// Update timing information
		float deltaTime = EngineTime::getDeltaTime();
		updateKeyTiming(deltaTime);
		updateMouseTiming(deltaTime);
	}

	void Input::onEvent(Event& e)
	{
        switch (e.type) 
        {
        case EventType::KeyPressed: 
        {
            int idx = remapKey(e.keyboard.key);
            if (idx < 0) break;  // Unknown key

            KeyState& state = s_keyStates[idx];

            // Only process if state actually changed
            if (!state.pressed) 
            {
                state.pressedThisFrame = true;
                state.timePressed = EngineTime::getTime();

                // Multi-tap detection
                float timeSinceLastTap = state.timePressed - state.lastTapTime;
                if (timeSinceLastTap < 0.3f) 
                {
                    state.tapCount++;
                }
                else 
                {
                    state.tapCount = 1;
                }
                state.lastTapTime = state.timePressed;
            }

            state.pressed = true;
            state.pressCountThisFrame++;
            s_lastKeyPressed = e.keyboard.key;
            break;
        }

        case EventType::KeyReleased: 
        {
            int idx = remapKey(e.keyboard.key);
            if (idx < 0) break;

            KeyState& state = s_keyStates[idx];
            state.pressed = false;
            state.releasedThisFrame = true;
            state.releaseCountThisFrame++;
            state.timeReleased = EngineTime::getTime();
            break;
        }

        case EventType::MouseMoved: 
        {
            if (SDL_GetWindowRelativeMouseMode(SDL_GetKeyboardFocus()))
            {
                // In relative mode, use delta directly from event
                s_mouseDeltaX = e.mouseMove.deltaX;
                s_mouseDeltaY = e.mouseMove.deltaY;
            }
            else
            {
                // In normal mode, calcullate delta from position
                s_mouseDeltaX += e.mouseMove.x - s_mouseX;
                s_mouseDeltaY += e.mouseMove.y - s_mouseY;
                s_mouseX = e.mouseMove.x;
                s_mouseY = e.mouseMove.y;
            }
            break;
        }

        case EventType::MouseButtonPressed: 
        {
            if (e.mouseButton.button >= 8) break;  // Out of bounds

            MouseButtonState& state = s_mouseStates[e.mouseButton.button];
            if (!state.pressed) {
                state.pressedThisFrame = true;
                state.timePressed = EngineTime::getTime();
            }
            state.pressed = true;
            break;
        }

        case EventType::MouseButtonReleased: 
        {
            if (e.mouseButton.button >= 8) break;

            MouseButtonState& state = s_mouseStates[e.mouseButton.button];
            state.pressed = false;
            state.releasedThisFrame = true;
            state.timeReleased = EngineTime::getTime();
            break;
        }

        case EventType::MouseScrolled: 
        {
            s_mouseWheelDelta += e.scroll.deltaY;
            break;
        }

        case EventType::WindowFocus: 
        {
            s_mouseInWindow = true;
            break;
        }

        case EventType::WindowLostFocus: 
        {
            s_mouseInWindow = false;
            // Release all keys when losing focus to prevent stuck keys
            for (auto& state : s_keyStates) 
            {
                state.pressed = false;
            }
            for (auto& state : s_mouseStates) 
            {
                state.pressed = false;
            }
            break;
        }

        default:
            // Events not relevant to Input system (WindowClose, WindowResize, None, etc.)
            // Silently ignore - these are handled by Application layer
            break;
        }
	}

    // Keyboard Implementation

    bool Input::isKeyPressed(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;
        return s_keyStates[idx].pressed;
    }

    bool Input::isKeyJustPressed(keyCode key, bool consume)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;

        KeyState& state = s_keyStates[idx];

        // Check: pressed this frame and not already consumed
        if (!state.pressedThisFrame || state.consumedPress)
        {
            return false;
        }

        if (consume)
        {
            state.consumedPress = true; // Mark as handled
        }

        return true;
    }

    bool Input::isKeyJustReleased(keyCode key, bool consume)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;

        KeyState& state = s_keyStates[idx];

        if (!state.releasedThisFrame || state.consumedRelease)
        {
            return false;
        }

        if (consume)
        {
            state.consumedRelease = true;
        }

        return true;
    }

    // Mouse Implementation

    bool Input::isMouseButtonPressed(MouseButton button)
    {
        int idx = static_cast<int>(button);
        if (idx >= 8) return false;
        return s_mouseStates[idx].pressed;
    }

    bool Input::isMouseButtonJustPressed(MouseButton button, bool consume)
    {
        int idx = static_cast<int>(button);
        if (idx >= 8) return false;

        MouseButtonState& state = s_mouseStates[idx];

        if (!state.pressedThisFrame || state.consumedPress)
        {
            return false;
        }

        if (consume)
        {
            state.consumedPress = true;
        }

        return true;
    }

    bool Input::isMouseButtonJustReleased(MouseButton button, bool consume)
    {
        int idx = static_cast<int>(button);
        if (idx >= 8) return false;

        MouseButtonState& state = s_mouseStates[idx];

        if (!state.releasedThisFrame || state.consumedRelease)
        {
            return false;
        }

        if (consume)
        {
            state.consumedRelease = true;
        }

        return true;
    }

    void Input::setMousePosition(float x, float y)
    {
        SDL_Window* window = SDL_GetKeyboardFocus();
        if (window)
        {
            SDL_WarpMouseInWindow(window, x,y);
            s_mouseX = x;
            s_mouseY = y;
            // Reset delta to prevent jump
            s_mouseDeltaX = 0;
            s_mouseDeltaY = 0;
        }
    }

    
    void Input::setMouseCursorVisible(bool visible)
    {
        if (visible)
        {
            SDL_ShowCursor();
        }
        else
        {
            SDL_HideCursor();
        }
    }
    

    
    void Input::setMouseCursorLocked(bool locked)
    {
        SDL_Window* window = SDL_GetKeyboardFocus();
        if (window)
        {
            SDL_SetWindowRelativeMouseMode(window, locked);

            // Clear any accumulated motion when switching modes
            if (locked)
            {
                SDL_GetRelativeMouseState(nullptr, nullptr);
            }
        }
    }

    // Timing Implementation

    float Input::getKeyHeldDuration(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0 || !s_keyStates[idx].pressed) return 0.0f;
        return EngineTime::getTime() - s_keyStates[idx].timePressed;
    }

    float Input::getTimeSinceKeyPressed(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0) return -1.0f;
        return EngineTime::getTime() - s_keyStates[idx].timePressed;
    }

    float Input::getTimeSinceKeyReleased(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0) return -1.0f;
        return EngineTime::getTime() - s_keyStates[idx].timeReleased;
    }

    bool Input::wasKeyDoubleTapped(keyCode key, float window)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;

        KeyState& state = s_keyStates[idx];

        // Must be pressed this frame with 2 taps
        if (state.pressedThisFrame && state.tapCount >= 2)
        {
            // Check if within timing window
            float timeSinceFirstTap = EngineTime::getTime() - (state.lastTapTime - window);
            return timeSinceFirstTap <= window;
        }
        return false;
    }

    bool Input::wasKeyTripleTapped(keyCode key, float window)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;

        KeyState& state = s_keyStates[idx];
        return state.pressedThisFrame && state.tapCount >= 3 && (EngineTime::getTime() - state.lastTapTime) <= window;
    }

    int Input::getKeyTapCount(keyCode key, float window)
    {
        int idx = remapKey(key);
        if (idx < 0) return 0;

        KeyState& state = s_keyStates[idx];
        if ((EngineTime::getTime() - state.lastTapTime) <= window)
        {
            return state.tapCount;
        }
        return 0;
    }

    bool Input::hasKeyStateChangedThisFrame(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0) return false;

        KeyState& state = s_keyStates[idx];
        return state.pressedThisFrame || state.releasedThisFrame;
    }

    int Input::getKeyPressCountThisFrame(keyCode key)
    {
        int idx = remapKey(key);
        if (idx < 0) return 0;
        return s_keyStates[idx].pressCountThisFrame;
    }

    // Action Mapping Implementaion

    void Input::bindAction(const std::string& action, keyCode key)
    {
        s_keyActions[action] = key;
        LOG_INFO("Bound action '{}' to key '{}'", action, getKeyName(key));
    }

    void Input::bindAction(const std::string& action, MouseButton button)
    {
        s_mouseActions[action] = static_cast<keyCode>(button);
        LOG_INFO("Bound acton '{}' to mouse button '{}'", action, static_cast<int>(button));
    }
    
    void Input::unbindAction(const std::string& action)
    {
        s_keyActions.erase(action);
        s_mouseActions.erase(action);
    }

    bool Input::isActionPressed(const std::string& action)
    {
        // Check keyboard binding
        auto keyIt = s_keyActions.find(action);
        if (keyIt != s_keyActions.end() && isKeyPressed(keyIt->second))
        {
            return true;
        }

        // Check mouse binding
        auto mouseIt = s_mouseActions.find(action);
        if (mouseIt != s_mouseActions.end())
        {
            MouseButton button = static_cast<MouseButton>(mouseIt->second);
            if (isMouseButtonPressed(button))
            {
                return true;
            }
        }
        return false;
    }

    bool Input::isActionJustPressed(const std::string& action, bool consume)
    {
        auto keyIt = s_keyActions.find(action);
        if (keyIt != s_keyActions.end() && isKeyJustPressed(keyIt->second, consume))
        {
            return true;
        }

        auto mouseIt = s_mouseActions.find(action);
        if (mouseIt != s_mouseActions.end())
        {
            MouseButton button = static_cast<MouseButton>(mouseIt->second);
            if (isMouseButtonJustPressed(button, consume))
            {
                return true;
            }
        }
        return false;
    }

    void Input::saveBindings(const std::string& filename)
    {
        std::ofstream file(filename);
        if (!file.is_open()) 
        {
            LOG_ERROR("Failed to save input bindings to {}", filename);
            return;
        }

        // Simple format: ActionName,Type,Value
        for (const auto& [action, key] : s_keyActions) 
        {
            file << action << ",Key," << key << "\n";
        }

        for (const auto& [action, button] : s_mouseActions) 
        {
            file << action << ",Mouse," << button << "\n";
        }

        file.close();
        LOG_INFO("Saved input bindings to {}", filename);
    }

    void Input::loadBindings(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open()) 
        {
            LOG_WARN("Failed to load input bindings from {} - using defaults", filename);
            return;
        }

        s_keyActions.clear();
        s_mouseActions.clear();

        std::string line;
        while (std::getline(file, line)) 
        {
            std::stringstream ss(line);
            std::string action, type, value;

            if (std::getline(ss, action, ',') &&
                std::getline(ss, type, ',') &&
                std::getline(ss, value, ',')) {

                if (type == "Key") 
                {
                    s_keyActions[action] = static_cast<keyCode>(std::stoi(value));
                }
                else if (type == "Mouse") 
                {
                    s_mouseActions[action] = static_cast<keyCode>(std::stoi(value));
                }
            }
        }

        file.close();
        LOG_INFO("Loaded input bindings from {}", filename);
    }

    std::string Input::getKeyName(keyCode key)
    {
        const char* name = SDL_GetKeyName(key);
        return name ? name : "Unknown";
    }

    // Internal Helpers

    void Input::updateKeyTiming(float deltaTime)
    {
        for (auto& state : s_keyStates)
        {
            if (state.pressed)
            {
                state.heldDuration += deltaTime;
            }
            else
            {
                state.heldDuration = 0.0f;
            }
        }
    }

    void Input::updateMouseTiming(float deltaTime)
    {
        for (auto& state : s_mouseStates)
        {
            if (state.pressed)
            {
                state.heldDuration += deltaTime;
            }
            else
            {
                state.heldDuration = 0.0f;
            }
        }
    }

    int Input::remapKey(keyCode key) 
    {

        // ASCII keys (0-127) map directly
        if (key >= 0 && key < 128) return key;

        // Special keys get compacted into 128-255 range
        // This saves ~1GB of memory vs direct indexing

        switch (key) 
        {
            // Navigation keys (128-131)
        case SDLK_UP:    return 128;
        case SDLK_DOWN:  return 129;
        case SDLK_LEFT:  return 130;
        case SDLK_RIGHT: return 131;

            // Modifier keys (132-139)
        case SDLK_LSHIFT: return 132;
        case SDLK_RSHIFT: return 133;
        case SDLK_LCTRL:  return 134;
        case SDLK_RCTRL:  return 135;
        case SDLK_LALT:   return 136;
        case SDLK_RALT:   return 137;
        case SDLK_LGUI:   return 138;  // Windows/Command key
        case SDLK_RGUI:   return 139;

            // Function keys (140-151)
        case SDLK_F1:  return 140;
        case SDLK_F2:  return 141;
        case SDLK_F3:  return 142;
        case SDLK_F4:  return 143;
        case SDLK_F5:  return 144;
        case SDLK_F6:  return 145;
        case SDLK_F7:  return 146;
        case SDLK_F8:  return 147;
        case SDLK_F9:  return 148;
        case SDLK_F10: return 149;
        case SDLK_F11: return 150;
        case SDLK_F12: return 151;

            // Important keys (155-169)
        case SDLK_ESCAPE:    return 155;
        case SDLK_TAB:       return 156;
        case SDLK_RETURN:    return 157;
        case SDLK_BACKSPACE: return 158;
        case SDLK_DELETE:    return 159;
        case SDLK_INSERT:    return 160;
        case SDLK_HOME:      return 161;
        case SDLK_END:       return 162;
        case SDLK_PAGEUP:    return 163;
        case SDLK_PAGEDOWN:  return 164;
        case SDLK_PAUSE:     return 165;
        case SDLK_PRINTSCREEN: return 166;
        case SDLK_CAPSLOCK:  return 167;

            // Numpad (170-185)
        case SDLK_KP_0: return 170;
        case SDLK_KP_1: return 171;
        case SDLK_KP_2: return 172;
        case SDLK_KP_3: return 173;
        case SDLK_KP_4: return 174;
        case SDLK_KP_5: return 175;
        case SDLK_KP_6: return 176;
        case SDLK_KP_7: return 177;
        case SDLK_KP_8: return 178;
        case SDLK_KP_9: return 179;
        case SDLK_KP_PLUS: return 180;
        case SDLK_KP_MINUS: return 181;
        case SDLK_KP_MULTIPLY: return 182;
        case SDLK_KP_DIVIDE: return 183;
        case SDLK_KP_ENTER: return 184;
        case SDLK_KP_PERIOD: return 185;

            // Media keys (190-195)
        case SDLK_MUTE: return 190;
        case SDLK_MEDIA_NEXT_TRACK: return 191;
        case SDLK_MEDIA_PREVIOUS_TRACK: return 192;
        case SDLK_MEDIA_PLAY: return 193;
        case SDLK_MEDIA_STOP: return 194;

            // Add more keys as needed (we have up to index 255)

        default: 
        {
            // Unknown key - log once per key
            static std::unordered_map<keyCode, bool> unknownKeys;
            if (unknownKeys.find(key) == unknownKeys.end()) 
            {
                LOG_WARN("Unknown key code: {} ({})", key, SDL_GetKeyName(key));
                unknownKeys[key] = true;
            }
            return -1;
        }

        }
    }
}