#include "core/EngineTime.h"
#include "core/Logger.h"

namespace Engine
{
    // Static member definitions
    EngineTime::TimePoint EngineTime::s_startTime;
    EngineTime::TimePoint EngineTime::s_lastFrameTime;

    double EngineTime::s_time = 0.0;
    double EngineTime::s_deltaTime = 0.0;
    double EngineTime::s_frameTime = 0.0;
    double EngineTime::s_fixedAccumulator = 0.0;
    double EngineTime::s_fpsAccumulator = 0.0;

    uint32_t EngineTime::s_fps = 0;
    uint32_t EngineTime::s_frameCount = 0;

    void EngineTime::init()
    {
        s_startTime = Clock::now();
        s_lastFrameTime = s_startTime;
    }

    void EngineTime::update()
    {
        TimePoint currentTime = Clock::now();

        // Delta time in seconds
        std::chrono::duration<double> delta = currentTime - s_lastFrameTime;
        s_deltaTime = delta.count();
        s_lastFrameTime = currentTime;

        // Clamp delta time to prevent spiral of death
        constexpr double MAX_DELTA = 0.1;
        if (s_deltaTime > MAX_DELTA)
        {
            LOG_WARN("Large frame time detected: {:.2f}ms (clamped to {:.2f}ms)", s_deltaTime * 1000.0, MAX_DELTA * 1000.0);
            s_deltaTime = MAX_DELTA;
        }

        // Total elapsed time
        std::chrono::duration<double> elapsed = currentTime - s_startTime;
        s_time = elapsed.count();

        // Frame time in ms
        s_frameTime = s_deltaTime * 1000.0;

        // FPS calculation
        s_frameCount++;
        s_fpsAccumulator += s_deltaTime;

        if (s_fpsAccumulator >= 1.0)
        {
            s_fps = s_frameCount;
            s_frameCount = 0;
            s_fpsAccumulator = 0.0;
        }
    }
}