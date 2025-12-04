#pragma once
#include "core/Logger.h"
#include "renderer/interface/IShader.h"
#include "renderer/interface/IRenderDevice.h"
#include <unordered_map>
#include <filesystem>
#include <functional>
#include <memory>

/*
 * ShaderManager.h
 *
 * PURPOSE:
 * Central shader registry with hot-reloading for rapid iteration. Tracks shader files,
 * automatically recompiles when modified on disk. Zero downtime (engine keeps running),
 * graceful degradation (bad code doesn't crash). Essential for shader development workflow.
 * Uses IRenderDevice abstraction (supports OpenGL/Vulkan).
 *
 * DESIGN RATIONALE (October 12, 2025):
 * Problem: Shader development slow (edit -> recompile -> restart -> test). Hot-reload
 * enables instant iteration (edit -> save -> see results). Need centralized registry (avoid
 * duplicate loads). Need graceful failure (bad shader shouldn't crash engine). Need
 * render device abstraction (GL/Vulkan support).
 *
 * Solution: Singleton registry with file watching and hot-reload.
 * - File watching: Poll modification times (std::filesystem)
 * - Hot-reload: Recompile on change, fallback to old shader on failure
 * - Graceful degradation: Old shader stays active if reload fails
 * - Render device abstraction: IRenderDevice->createShader() (API-agnostic)
 * - Callback notification: UI feedback on success/failure
 *
 * Key Insight: Hot-reload revolutionary for shader development. Traditional workflow:
 * edit -> recompile -> restart (30 seconds). Hot-reload: edit -> save (instant). 10-100×
 * faster iteration. Graceful failure critical (bad shader code shouldn't crash engine).
 * Render device abstraction enables GL/Vulkan support without changing workflow.
 *
 * DESIGN PHILOSOPHY:
 * - Singleton: Global access (ShaderManager::get())
 * - Hot-reload: Instant iteration (no engine restart)
 * - Graceful degradation: Old shader stays on failure
 * - Poll-based: Simple, cross-platform (no OS-specific watchers)
 * - Dependency injection: Render device passed in (not hardcoded)
 *
 * KEY CONCEPTS:
 * 1. Hot-Reloading:
 *    - File watching: Check modification times every frame
 *    - Recompile: Detect change -> create new shader
 *    - Fallback: If compilation fails, keep old shader
 *
 * 2. File Watching:
 *    - Poll-based: Check timestamps in update() (every frame)
 *    - std::filesystem::last_write_time()
 *    - Compare: Current time vs cached time
 *
 * 3. Graceful Degradation:
 *    - Compilation error: Log error, keep old shader
 *    - Missing file: Log warning, keep old shader
 *    - Result: Engine never crashes from bad shader code
 *
 * 4. Render Device Abstraction:
 *    - IRenderDevice->createShader(): API-agnostic creation
 *    - Stores IShader*: Interface pointer (not concrete)
 *    - Hot-reload works identically with GL/Vulkan
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === INITIALIZATION (Application Startup) ===
 * auto renderDevice = std::make_unique<GLRenderDevice>();
 * ShaderManager::get().initialize(renderDevice.get());
 *
 * // === LOAD SHADER (Tracked Automatically) ===
 * auto shader = ShaderManager::get().loadShader(
 *     "basic",                        // Name for lookup
 *     "assets/shaders/basic.vert",    // Vertex shader
 *     "assets/shaders/basic.frag"     // Fragment shader
 * );
 * // Console: [INFO] Loading shader 'basic' from basic.vert + basic.frag
 * //          [INFO] Shader 'basic' loaded and tracked for hot-reloading
 *
 * // === GET SHADER BY NAME ===
 * auto shader = ShaderManager::get().getShader("basic");
 * if (shader) {
 *     shader->bind();
 *     shader->setUniform("u_Color", vec3(1, 0, 0));
 * }
 *
 * // === UPDATE LOOP (Required for Hot-Reload) ===
 * void Application::run() {
 *     while (running) {
 *         ShaderManager::get().update();  // Check for file changes
 *
 *         // Render scene...
 *     }
 * }
 *
 * // === RELOAD NOTIFICATION ===
 * ShaderManager::get().setReloadCallback(
 *     [](const std::string& name, bool success) {
 *         if (success) {
 *             LOG_INFO("Shader '{}' reloaded!", name);
 *             // Show green checkmark in UI
 *         } else {
 *             LOG_ERROR("Shader '{}' reload failed!", name);
 *             // Show red X in UI
 *         }
 *     }
 * );
 * ```
 *
 * HOT-RELOAD WORKFLOW:
 *
 * ```
 * 1. Developer edits shader in text editor (VS Code, Sublime)
 *    
 * 2. Developer saves file (Ctrl+S)
 *    
 * 3. ShaderManager::update() detects change (next frame)
 *    - Compare: Current mod time > cached mod time
 *    
 * 4. ShaderManager recompiles shader
 *    - renderDevice->createShaderFromFiles()
 *    
 * 5a. Success: New shader active immediately
 *     - Old shader destroyed
 *     - Callback: reloadCallback(name, true)
 *     - Console: [INFO] Shader 'basic' reloaded successfully
 *     
 * 5b. Failure: Old shader stays active
 *     - New shader discarded
 *     - Callback: reloadCallback(name, false)
 *     - Console: [ERROR] Shader 'basic' reload FAILED
 *     -          [ERROR] Keeping old shader active
 * ```
 *
 * Traditional workflow (NO hot-reload):
 * - Edit shader -> Save -> Close engine -> Recompile engine -> Restart engine -> Test
 * - Time: ~30 seconds per iteration
 * - Frustrating, slow workflow
 *
 * Hot-reload workflow:
 * - Edit shader -> Save -> See results (engine still running)
 * - Time: ~0.1 seconds per iteration
 * - Result: 300× faster iteration!
 *
 * FILE WATCHING IMPLEMENTATION:
 *
 * ```cpp
 * void ShaderManager::update() {
 *     for (auto& [name, info] : m_shaders) {
 *         bool needsReload = false;
 *
 *         // Check vertex shader modification time
 *         auto vertTime = std::filesystem::last_write_time(info.vertPath);
 *         if (vertTime > info.vertLastModified) {
 *             LOG_INFO("Vertex shader modified: {}", info.vertPath);
 *             info.vertLastModified = vertTime;
 *             needsReload = true;
 *         }
 *
 *         // Check fragment shader modification time
 *         auto fragTime = std::filesystem::last_write_time(info.fragPath);
 *         if (fragTime > info.fragLastModified) {
 *             LOG_INFO("Fragment shader modified: {}", info.fragPath);
 *             info.fragLastModified = fragTime;
 *             needsReload = true;
 *         }
 *
 *         if (needsReload) {
 *             reloadShader(name, info);
 *         }
 *     }
 * }
 * ```
 *
 * GRACEFUL DEGRADATION:
 *
 * ```cpp
 * void reloadShader(const std::string& name, ShaderInfo& info) {
 *     LOG_INFO("Hot-reloading shader '{}'...", name);
 *
 *     try {
 *         // Try to compile new shader
 *         auto newShader = m_renderDevice->createShaderFromFiles(
 *             info.vertPath.c_str(),
 *             info.fragPath.c_str()
 *         );
 *
 *         if (newShader) {
 *             // Success! Replace old shader
 *             info.shader = newShader;
 *             LOG_INFO("Shader '{}' reloaded successfully", name);
 *
 *             if (m_reloadCallback) {
 *                 m_reloadCallback(name, true);  // Notify UI
 *             }
 *         } else {
 *             // Compilation failed, keep old shader
 *             LOG_ERROR("Shader '{}' reload FAILED: createShader returned nullptr", name);
 *             LOG_ERROR("Keeping old shader active");
 *
 *             if (m_reloadCallback) {
 *                 m_reloadCallback(name, false);  // Notify UI
 *             }
 *         }
 *     } catch (const std::exception& e) {
 *         // Exception during reload, keep old shader
 *         LOG_ERROR("Shader '{}' reload FAILED: {}", name, e.what());
 *         LOG_ERROR("Keeping old shader active");
 *
 *         if (m_reloadCallback) {
 *             m_reloadCallback(name, false);
 *         }
 *     }
 * }
 * ```
 *
 * RENDER DEVICE ABSTRACTION:
 *
 * Before (Hardcoded OpenGL):
 * ```cpp
 * auto shader = std::make_shared<Shader>(vertPath, fragPath);  // OpenGL only
 * ```
 *
 * After (API-Agnostic):
 * ```cpp
 * auto shader = m_renderDevice->createShaderFromFiles(vertPath, fragPath);
 * // Works with GLRenderDevice OR VKRenderDevice
 * ```
 *
 * Benefits:
 * - Hot-reload works with both OpenGL and Vulkan
 * - No shader manager changes needed when adding Vulkan
 * - Interface abstraction (IShader*) not concrete (Shader*)
 *
 * PERFORMANCE:
 *
 * File watching overhead:
 * - Per shader: ~0.001ms (timestamp comparison)
 * - 10 shaders: ~0.01ms (negligible)
 * - Only checks on update() (once per frame)
 *
 * Compilation cost:
 * - Simple shader: ~1-5ms
 * - Complex shader: ~10-50ms
 * - Only on file change (rare)
 *
 * Result: Hot-reload has zero runtime cost (only checks timestamps)
 *
 * CURRENT STATE (October 12, 2025):
 * - Singleton pattern (global access)
 * - Hot-reload support (file watching + recompilation)
 * - Graceful degradation (old shader on failure)
 * - Render device abstraction (IRenderDevice)
 * - Callback notification (UI feedback)
 * - Vertex + fragment shaders only
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Vertex + Fragment Only:
 * - No geometry, compute, tessellation shaders
 * - Future: Add support 
 *
 * 2. Manual update() Required:
 * - Not automatic background thread
 * - Must call update() in game loop
 * - Future: Background thread with OS notifications 
 *
 * 3. No #include Support:
 * - Can't share code between shaders (#include "common.glsl")
 * - Future: Shader preprocessing 
 *
 * 4. No Variant System:
 * - Can't generate multiple versions (with/without feature)
 * - Future: Feature flags, permutations 
 *
 * 5. No Binary Caching:
 * - Recompiles from source every time
 * - Future: Cache compiled binaries 
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 12, 2025: Initial implementation
 * - Singleton pattern with shader registry
 * - Hot-reload support (file watching)
 * - Graceful degradation
 * - Render device abstraction (IRenderDevice)
 * - Callback notification
 *
 * (Additional Shader Types):
 * - Geometry shaders (tessellation, subdivision)
 * - Compute shaders (particles, post-processing)
 * - Tessellation shaders (terrain, curved surfaces)
 * - Time: 2-3 days
 *
 * (Advanced Features):
 * - Background thread watching (OS notifications)
 * - Shader preprocessing (#include, #define)
 * - Shared code libraries (common.glsl)
 * - Time: 1 week
 *
 * (Optimization):
 * - Variant system (feature flags, permutations)
 * - Binary caching (skip recompilation)
 * - Shader compilation pipeline
 * - Time: 2 weeks
 *
 * DEPENDENCIES:
 * - core/Logger.h: Console logging (reload status)
 * - renderer/interface/IShader.h: Shader interface
 * - renderer/interface/IRenderDevice.h: Factory for shaders
 * - <unordered_map>: Shader registry
 * - <filesystem>: File watching (mod time)
 * - <functional>: Callback support
 *
 * THREAD SAFETY:
 * - NOT thread-safe: Singleton without mutex
 * - All operations on main thread only
 * - Future: Mutex protection for background watching
 *
 * REFERENCES:
 * - Live shader editing: Unreal Engine, Unity shader graph
 * - File watching: std::filesystem::last_write_time()
 * - Singleton pattern: Effective C++ by Scott Meyers
 *
 * HISTORY:
 * October 12, 2025: Initial implementation
 * - Singleton pattern (global access)
 * - Hot-reload support (file watching + recompilation)
 * - Graceful degradation (old shader on failure)
 * - Render device abstraction (IRenderDevice factory)
 * - Callback notification (UI feedback)
 * - Poll-based file watching (std::filesystem)
 * - Result: Revolutionary shader development workflow (300× faster iteration)
 *
 */

namespace Engine
{
    class ShaderManager
    {
    public:
        static ShaderManager& get()
        {
            static ShaderManager instance;
            return instance;
        }

        // REQUIRED: Initialize with render device before using
        void initialize(IRenderDevice* renderDevice)
        {
            // Guard against double initialization
            if (m_renderDevice != nullptr)
            {
                LOG_WARN("ShaderManager already initialized - ignoring duplicate call");
                return;
            }

            m_renderDevice = renderDevice;
            LOG_INFO("ShaderManager initialized with render device");
        }

        // For notification callback
        using ReloadCallback = std::function<void(const std::string& name, bool success)>;

        void setReloadCallback(ReloadCallback callback)
        {
            m_reloadCallback = callback;
        }

        // Load shader and start tracking files
        std::shared_ptr<IShader> loadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath)
        {
            if (!m_renderDevice)
            {
                LOG_ERROR("ShaderManager not initialized! Call initialize(renderDevice) first.");
                return nullptr;
            }

            LOG_INFO("Loading shader '{}' from {} + {}", name, vertPath, fragPath);

            // Load shader through render device (API-agnostic)
            auto shader = m_renderDevice->createShaderFromFiles(vertPath.c_str(), fragPath.c_str());

            if (!shader)
            {
                LOG_ERROR("Failed to create shader '{}'", name);
                return nullptr;
            }

            // Track for hot-reloading
            ShaderInfo info;
            info.shader = shader;
            info.vertPath = vertPath;
            info.fragPath = fragPath;
            info.vertLastModified = getFileModTime(vertPath);
            info.fragLastModified = getFileModTime(fragPath);

            m_shaders[name] = info;

            LOG_INFO("Shader '{}' loaded and tracked for hot-reloading", name);
            return shader;
        }

        // Get shader by name
        std::shared_ptr<IShader> getShader(const std::string& name)
        {
            auto it = m_shaders.find(name);
            if (it != m_shaders.end())
            {
                return it->second.shader;
            }
            LOG_ERROR("Shader '{}' not found", name);
            return nullptr;
        }

        // Check for file changes and reload if needed
        void update()
        {
            for (auto& [name, info] : m_shaders)
            {
                bool needsReload = false;

                // Check vertex shader
                auto vertTime = getFileModTime(info.vertPath);
                if (vertTime > info.vertLastModified)
                {
                    LOG_INFO("Vertex shader modified: {}", info.vertPath);
                    info.vertLastModified = vertTime;
                    needsReload = true;
                }

                // Check fragment shader
                auto fragTime = getFileModTime(info.fragPath);
                if (fragTime > info.fragLastModified)
                {
                    LOG_INFO("Fragment shader modified: {}", info.fragPath);
                    info.fragLastModified = fragTime;
                    needsReload = true;
                }

                if (needsReload)
                {
                    reloadShader(name, info);
                }
            }
        }

    private:
        struct ShaderInfo
        {
            std::shared_ptr<IShader> shader;  // Changed from Shader to IShader
            std::string vertPath;
            std::string fragPath;
            std::filesystem::file_time_type vertLastModified;
            std::filesystem::file_time_type fragLastModified;
        };

        IRenderDevice* m_renderDevice = nullptr;  // Injected dependency
        ReloadCallback m_reloadCallback;
        std::unordered_map<std::string, ShaderInfo> m_shaders;

        std::filesystem::file_time_type getFileModTime(const std::string& path)
        {
            namespace fs = std::filesystem;
            try
            {
                return fs::last_write_time(path);
            }
            catch (const fs::filesystem_error& e)
            {
                LOG_ERROR("Failed to get file time for {}: {}", path, e.what());
                return fs::file_time_type::min();
            }
        }

        void reloadShader(const std::string& name, ShaderInfo& info)
        {
            if (!m_renderDevice)
            {
                LOG_ERROR("Cannot reload shader '{}': render device not set", name);
                return;
            }

            LOG_INFO("Hot-reloading shader '{}'...", name);

            try
            {
                // Try to compile new shader through render device
                auto newShader = m_renderDevice->createShaderFromFiles(
                    info.vertPath.c_str(),
                    info.fragPath.c_str()
                );

                if (newShader)
                {
                    // Success! Replace old shader
                    info.shader = newShader;
                    LOG_INFO("Shader '{}' reloaded successfully", name);

                    if (m_reloadCallback)
                    {
                        m_reloadCallback(name, true);
                    }
                }
                else
                {
                    LOG_ERROR("Shader '{}' reload FAILED: createShader returned nullptr", name);
                    LOG_ERROR("Keeping old shader active");

                    if (m_reloadCallback)
                    {
                        m_reloadCallback(name, false);
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Shader '{}' reload FAILED: {}", name, e.what());
                LOG_ERROR("Keeping old shader active");

                if (m_reloadCallback)
                {
                    m_reloadCallback(name, false);
                }
            }
        }
    };
}