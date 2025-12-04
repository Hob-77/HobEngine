#pragma once
#include <chrono>

/*
 * EngineTime.h
 *
 * PURPOSE:
 * Frame-based timing system providing delta time, fixed timestep, and FPS tracking.
 * Central timing authority for animations, physics, input, and performance monitoring.
 * Abstracts platform-specific high-resolution timers through std::chrono. Clean global
 * interface (no hardcoded timesteps in main loop).
 *
 * DESIGN RATIONALE (September 23, 2025):
 * Problem: Need consistent time across all systems (animations, physics, input). Last
 * engine (2D) had hardcoded timestep in main loop (bad, inflexible). Need variable
 * timestep (smooth rendering) AND fixed timestep (stable physics). Need precision
 * (prevent drift) but convenient API (float for gameplay). Need FPS tracking
 * (performance monitoring).
 *
 * Solution: Static time system with dual timestep support.
 * - Variable timestep: Adaptive framerate (smooth rendering)
 * - Fixed timestep: 60 Hz physics (deterministic, stable)
 * - Double precision: Prevent drift over hours
 * - Float API: Convenient for gameplay (standard)
 * - FPS tracking: Performance monitoring
 * - Result: Clean abstraction, no hardcoded timesteps
 *
 * Key Insight: Time fundamental to game engines (everything needs delta). Static
 * interface natural (global time authority, no passing Time& everywhere). Dual timestep
 * essential (variable for rendering, fixed for physics). Learned from last engine (2D):
 * Hardcoded timestep in main loop inflexible, error-prone. Glenn Fiedler's "Fix Your
 * Timestep" article critical (fixed timestep prevents physics instability).
 *
 * DESIGN PHILOSOPHY:
 * - Static interface: Global time authority (no instances)
 * - Variable timestep: Smooth rendering, adaptive framerate
 * - Fixed timestep: Stable physics, deterministic (60 Hz)
 * - Double precision core: Prevent drift, sub-ms accuracy
 * - Float API: Convenient gameplay (7 digits sufficient)
 * - Delta clamping: Stability over accuracy (max 100ms)
 *
 * KEY CONCEPTS:
 * 1. Variable Timestep (getDeltaTime):
 *    - Varies with framerate (16ms @ 60fps, 8ms @ 120fps)
 *    - Use for: Rendering, animations, particles, UI, camera
 *    - Formula: position += velocity * deltaTime
 *
 * 2. Fixed Timestep (getFixedDeltaTime):
 *    - Always 1/60 second (16.666ms) regardless of framerate
 *    - Use for: Physics simulation, deterministic gameplay
 *    - Prevents physics instability (tunneling, energy drift)
 *    - "Fix Your Timestep" pattern (Glenn Fiedler)
 *
 * 3. Precision Strategy:
 *    - Internal: Double (~15 digits, prevent drift)
 *    - Public API: Float (~7 digits, convenient)
 *    - High-precision: getTimeDouble(), getDeltaTimeDouble()
 *
 * 4. Delta Clamping:
 *    - Max 100ms (10 FPS minimum)
 *    - Prevents "spiral of death" (physics explosion)
 *    - Handles hitches (loading, GC, alt-tab, debugger)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION ===
 * int main() {
 *     EngineTime::init();
 *
 *     while (running) {
 *         // Update time
 *         EngineTime::update();
 *
 *         // Variable timestep (rendering, animations)
 *         float deltaTime = EngineTime::getDeltaTime();
 *         onUpdate(deltaTime);
 *
 *         // Fixed timestep (physics)
 *         EngineTime::updateFixed();
 *         while (EngineTime::shouldDoFixedUpdate()) {
 *             float fixedDelta = EngineTime::getFixedDeltaTime();
 *             onFixedUpdate(fixedDelta);  // Always 1/60 second
 *             EngineTime::consumeFixedUpdate();
 *         }
 *
 *         // Rendering
 *         onRender();
 *     }
 * }
 *
 * // === VARIABLE TIMESTEP (RENDERING) ===
 * void Player::update(float deltaTime) {
 *     // Smooth, adaptive to framerate
 *     position += velocity * deltaTime;
 *     animator.update(deltaTime);
 *     particles.update(deltaTime);
 *     camera.update(deltaTime);
 * }
 *
 * // === FIXED TIMESTEP (PHYSICS) ===
 * void PhysicsWorld::update(float fixedDelta) {
 *     // fixedDelta is ALWAYS 1/60 (0.01666...)
 *     // Stable, deterministic
 *     for (auto& body : bodies) {
 *         body.velocity += gravity * fixedDelta;
 *         body.position += body.velocity * fixedDelta;
 *     }
 *
 *     collisionSystem.update(fixedDelta);
 * }
 *
 * // === INTERPOLATION (SMOOTH RENDERING) ===
 * void GameObject::onFixedUpdate(float dt) {
 *     // Update physics state
 *     m_prevPos = m_currPos;
 *     m_currPos += m_velocity * dt;
 * }
 *
 * void GameObject::onRender() {
 *     // Interpolate between physics steps
 *     float alpha = EngineTime::getInterpolationAlpha();
 *     vec3 renderPos = lerp(m_prevPos, m_currPos, alpha);
 *     draw(renderPos);  // Smooth motion even at high FPS!
 * }
 *
 * // === PERFORMANCE MONITORING ===
 * void Application::update() {
 *     float fps = EngineTime::getFPS();
 *     if (fps < 30.0f) {
 *         LOG_WARN("Low framerate: {:.1f} FPS", fps);
 *     }
 *
 *     float frameMs = EngineTime::getDeltaTime() * 1000.0f;
 *     if (frameMs > 16.666f) {
 *         LOG_WARN("Frame budget exceeded: {:.2f}ms (target: 16.67ms)", frameMs);
 *     }
 * }
 *
 * // === HIGH-PRECISION TIMING ===
 * // Replay recording (needs exact timestamps)
 * double timestamp = EngineTime::getTimeDouble();
 * replayRecorder.recordEvent(timestamp, event);
 *
 * // Network synchronization (needs precise delta)
 * double preciseDeltatime = EngineTime::getDeltaTimeDouble();
 * networkSync.update(preciseDelta);
 * ```
 *
 * DUAL TIMESTEP SYSTEM - Why Both?
 *
 * Variable timestep (getDeltaTime):
 * - Varies with framerate
 * - 60 FPS: 16.666ms
 * - 120 FPS: 8.333ms
 * - 144 FPS: 6.944ms
 *
 * Advantages:
 * - Smooth rendering (no stutter)
 * - Responsive input (immediate feedback)
 * - Natural animations (adaptive speed)
 *
 * Disadvantages:
 * - Physics unstable (tunneling, energy drift)
 * - Non-deterministic (hard to reproduce bugs)
 * - Network desync (different framerates = different results)
 *
 * Fixed timestep (getFixedDeltaTime):
 * - Always 1/60 second (0.01666...)
 * - Runs multiple times per frame if needed
 * - Example: 144 FPS = ~2.4 physics steps per frame
 *
 * Advantages:
 * - Stable physics (no tunneling, consistent forces)
 * - Deterministic (same inputs = same results)
 * - Network friendly (all clients run same physics rate)
 *
 * Disadvantages:
 * - Visual stutter if not interpolated
 * - More complex (need accumulator pattern)
 *
 * Solution: Use both!
 * - Variable: Rendering, animations, UI, camera
 * - Fixed: Physics, gameplay logic, networking
 *
 * FIXED TIMESTEP IMPLEMENTATION - "Fix Your Timestep":
 *
 * ```cpp
 * // Glenn Fiedler's pattern
 * const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz
 * double accumulator = 0.0;
 *
 * void update() {
 *     double frameTime = getDeltaTime();
 *     accumulator += frameTime;
 *
 *     // Run physics in fixed steps
 *     while (accumulator >= FIXED_DT) {
 *         onFixedUpdate(FIXED_DT);
 *         accumulator -= FIXED_DT;
 *     }
 *
 *     // Interpolation alpha (for smooth rendering)
 *     float alpha = accumulator / FIXED_DT;
 * }
 * ```
 *
 * Examples at different framerates:
 * - 30 FPS (33.333ms frame): 2 physics steps
 * - 60 FPS (16.666ms frame): 1 physics step
 * - 120 FPS (8.333ms frame): ~0.5 physics steps (every other frame)
 * - 144 FPS (6.944ms frame): ~0.4 physics steps (2-3 frames per step)
 *
 * DELTA TIME CLAMPING - Preventing Disaster:
 *
 * Problem: Frame hitch (100ms spike)
 * ```cpp
 * position += velocity * 0.1f;  // Huge jump!
 * // Object teleports, physics explodes, game unplayable
 * ```
 *
 * Solution: Clamp maximum delta
 * ```cpp
 * float deltaTime = std::min(realDelta, 0.1f);  // Max 100ms (10 FPS)
 * position += velocity * deltaTime;  // Controlled movement
 * ```
 *
 * Causes of frame hitches:
 * - Asset loading (textures, models, levels)
 * - Garbage collection (memory allocation)
 * - Alt-tab (window loses focus)
 * - Debugger breakpoint (pauses execution)
 * - CPU spike (background process)
 *
 * Trade-off:
 * - Sacrifices accuracy (game slows down during hitches)
 * - Gains stability (physics doesn't explode)
 * - Better: Slow motion vs broken physics
 *
 * PRECISION STRATEGY - Double Core, Float API:
 *
 * Internal storage (double):
 * - ~15 significant digits
 * - Prevents drift over hours
 * - Example: 1 hour = 3,600,000 ms (7 digits) -> still 8 digits precision
 *
 * Public API (float):
 * - ~7 significant digits
 * - Sufficient per frame (max 1000ms = 4 digits)
 * - Standard in Unity, Unreal (familiar)
 * - Convenient for gameplay math
 *
 * High-precision accessors:
 * - getTimeDouble(): Exact timestamps (replay, network)
 * - getDeltaTimeDouble(): Precise measurements (profiling)
 *
 * FPS CALCULATION - Stable Display:
 *
 * Naive approach (too noisy):
 * ```cpp
 * float fps = 1.0f / deltaTime;
 * // 59.8, 60.2, 59.5, 60.7... (unreadable)
 * ```
 *
 * Better approach (frame counting):
 * ```cpp
 * frameCount++;
 * fpsTimer += deltaTime;
 *
 * if (fpsTimer >= 1.0f) {
 *     fps = frameCount / fpsTimer;  // Average over 1 second
 *     frameCount = 0;
 *     fpsTimer = 0.0f;
 * }
 * // 60, 60, 60, 60... (stable, readable)
 * ```
 *
 * FRAME LIFECYCLE - Complete Example:
 *
 * ```cpp
 * int main() {
 *     // 1. Initialize
 *     EngineTime::init();
 *
 *     while (running) {
 *         // 2. Update time (calculate delta, FPS)
 *         EngineTime::update();
 *
 *         // 3. Process events
 *         processEvents();
 *
 *         // 4. Variable timestep update (rendering)
 *         float deltaTime = EngineTime::getDeltaTime();
 *         game.update(deltaTime);
 *
 *         // 5. Fixed timestep update (physics)
 *         EngineTime::updateFixed();
 *         while (EngineTime::shouldDoFixedUpdate()) {
 *             float fixedDelta = EngineTime::getFixedDeltaTime();
 *             physics.update(fixedDelta);
 *             EngineTime::consumeFixedUpdate();
 *         }
 *
 *         // 6. Render
 *         renderer.render();
 *     }
 * }
 * ```
 *
 * TIMING PRECISION - Platform Details:
 *
 * std::chrono::high_resolution_clock:
 * - Windows: QueryPerformanceCounter (~microsecond precision)
 * - Linux: clock_gettime(CLOCK_MONOTONIC) (~nanosecond precision)
 * - macOS: mach_absolute_time() (~nanosecond precision)
 *
 * Practical precision:
 * - Double: Sub-millisecond accuracy for days
 * - Float: Good for per-frame (16.666ms has 5 digits precision)
 * - Fixed timestep: Exact 60 Hz (no accumulated error)
 *
 * CURRENT STATE (September 23, 2025):
 * - Static time system (global access)
 * - Variable timestep (getDeltaTime, adaptive framerate)
 * - Fixed timestep (getFixedDeltaTime, 60 Hz physics)
 * - Double precision core (prevent drift)
 * - Float convenience API (gameplay standard)
 * - Delta clamping (max 100ms, stability)
 * - FPS tracking (1-second window, stable display)
 * - Interpolation support (smooth rendering)
 * - std::chrono abstraction (cross-platform)
 * - Status: Production-ready, clean abstraction
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. No Time Scaling:
 * - Can't do slow-motion or fast-forward
 * - Future: setTimeScale(float) 
 *
 * 2. No Frame Rate Limiting:
 * - Relies on VSync (GPU-driven)
 * - Future: CPU-based frame limiter 
 *
 * 3. No Delta Smoothing:
 * - Only max clamp (no averaging)
 * - Future: Average last N frames 
 *
 * 4. Fixed Timestep Hardcoded:
 * - Always 60 Hz (can't change)
 * - Future: Configurable (30/60/120/240 Hz)
 *
 * 5. No Profiling Timers:
 * - No scoped timers, hierarchical profiling
 * - Future: ScopedTimer class 
 *
 * 6. No Pause/Resume:
 * - Can't pause time (menus, debugging)
 * - Future: setPaused(bool) 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * September 23, 2025: Initial implementation
 * - Static time system (global access)
 * - Dual timestep (variable + fixed)
 * - Double precision core, float API
 * - Delta clamping (max 100ms)
 * - FPS tracking (1-second window)
 * - Learned from last engine (no hardcoded timesteps)
 * - Status: Production-ready
 *
 * (Time Scaling):
 * - setTimeScale(float) for slow-motion
 * - Bullet time, fast-forward, etc.
 * - Time: 2-3 hours
 *
 * (Pause/Resume):
 * - setPaused(bool) for menus, debugging
 * - Time: 1-2 hours
 *
 * (Frame Rate Limiting):
 * - CPU-based FPS cap (30/60/120/144)
 * - Don't rely on VSync
 * - Time: 2-3 hours
 *
 * (Delta Smoothing):
 * - Average last N frames (reduce jitter)
 * - Configurable window size
 * - Time: 2-3 hours
 *
 * (Configurable Fixed Timestep):
 * - Choose 30/60/120/240 Hz
 * - Physics quality vs performance trade-off
 * - Time: 1-2 hours
 *
 * (Profiling Support):
 * - ScopedTimer class (RAII timing)
 * - Hierarchical profiling (nested timers)
 * - Time: 1-2 days
 *
 * DEPENDENCIES:
 * - <chrono>: std::chrono::high_resolution_clock (cross-platform timing)
 *
 * THREAD SAFETY:
 * - NOT thread-safe: No mutex protection
 * - All operations on main thread only (game loop)
 *
 * REFERENCES:
 * - Glenn Fiedler: "Fix Your Timestep" (essential article)
 * - Game Programming Patterns: Game Loop chapter
 * - Unity/Unreal documentation: Time.deltaTime
 * - Previous 2D engine 
 *
 * HISTORY:
 * September 23, 2025: Initial implementation
 * - Static time system (no hardcoded timesteps in main)
 * - Variable timestep (smooth rendering, adaptive framerate)
 * - Fixed timestep (stable physics, 60 Hz, "Fix Your Timestep")
 * - Double precision core (prevent drift over hours)
 * - Float convenience API (standard, familiar)
 * - Delta clamping (max 100ms, prevent spiral of death)
 * - FPS tracking (1-second window, stable display)
 * - Interpolation alpha (smooth rendering between physics steps)
 * - std::chrono abstraction (cross-platform, high precision)
 * - Learned from last engine: No hardcoded timesteps (inflexible, error-prone)
 * - Result: Clean abstraction, flexible, production-ready
 *
 * Note: This design is overengineered slighlty as I don't like the concept of having this done later
 *     - As I want to being able to use Deltatime and fixed timestep for physics + interpolation at anytime, also
 *     - this also helps us display our delta time as FPS (frames per a second)
 */

namespace Engine
{
    class EngineTime
    {
    public:
        static void init();
        static void update();

        // Variable timestep float normal
        static float getDeltaTime() { return static_cast<float>(s_deltaTime); }
        static float getTime() { return static_cast<float>(s_time); }
        static float getFrameTime() { return static_cast<float>(s_frameTime); }

        // Variable timestep double High-precision versions
        static double getDeltaTimeDouble() { return s_deltaTime; }
        static double getTimeDouble() { return s_time; }

        // Fixed timestep
        static void updateFixed() { s_fixedAccumulator += s_deltaTime; }
        static bool shouldDoFixedUpdate() { return s_fixedAccumulator >= FIXED_TIMESTEP; }
        static void consumeFixedUpdate() { s_fixedAccumulator -= FIXED_TIMESTEP; }
        static float getFixedDeltaTime() { return static_cast<float>(FIXED_TIMESTEP); }
        static float getInterpolationAlpha() { return static_cast<float>(s_fixedAccumulator / FIXED_TIMESTEP); }


        // Performance metrics
        static uint32_t getFPS() { return s_fps; }

    private:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        static TimePoint s_startTime;
        static TimePoint s_lastFrameTime;

        // Variable timestep
        static double s_time;        // Total time since start
        static double s_deltaTime;   // Time since last frame
        static double s_frameTime;   // Milliseconds per frame

        // Fixed timestep Glenn Fiedler method
        static constexpr double FIXED_TIMESTEP = 1.0 / 60.0;
        static double s_fixedAccumulator;

        // FPS calculation
        static uint32_t s_fps;
        static uint32_t s_frameCount;
        static double s_fpsAccumulator;
    };
}