#pragma once
#include "renderer/RenderQueue.h"
#include "renderer/interface/IShader.h"
#include "renderer/interface/IRenderer.h"
#include "core/Logger.h"

/*
 * RenderQueueManager.h
 *
 * PURPOSE:
 * Dual-queue manager for opaque and transparent rendering. Automatically routes objects
 * based on material transparency. Manages render order and state transitions. Achieves
 * 98%+ material batching efficiency. Integrates with IRenderer for state management
 * (November 14, 2025 refactor).
 *
 * DESIGN RATIONALE (October 26-28, 2025, Refactored November 6-14, 2025):
 * Problem: Opaque and transparent objects need different rendering strategies. Opaque
 * benefits from front-to-back sorting (early-Z). Transparent requires back-to-front
 * (correct blending). Manual queue management error-prone. State transitions scattered
 * throughout code (before November 14).
 *
 * Solution: Dual-queue manager with automatic routing and integrated state management.
 * - Opaque queue: Material-first sorting (batching), front-to-back (early-Z)
 * - Transparent queue: Distance-first sorting (blending), back-to-front (painter)
 * - Automatic routing: Based on material.isTransparent flag
 * - State management: IRenderer handles depth/blending (November 14 refactor)
 * - Result: 98%+ batching efficiency, correct rendering order, clean API
 *
 * Key Insight: Transparent objects fundamentally different from opaque (order-dependent).
 * Dual queues enable optimal sorting strategies for each. Automatic routing prevents
 * errors. Integrated state management via IRenderer (November 14) maintains abstraction.
 *
 * DESIGN PHILOSOPHY:
 * - Dual-queue: Separate strategies for opaque/transparent
 * - Automatic routing: Simple API, hard to misuse
 * - Integrated state: IRenderer manages depth/blending (abstracted)
 * - Statistics aggregation: Combined view of both queues
 * - Submit-sort-render-clear: Clear lifecycle pattern
 *
 * KEY CONCEPTS:
 * 1. Dual-Queue Architecture:
 *    - Opaque: Front-to-back (material -> distance)
 *    - Transparent: Back-to-front (distance -> material)
 *    - Separate queues: Different sorting strategies
 *
 * 2. Automatic Routing:
 *    - material.isTransparent -> Transparent queue
 *    - !material.isTransparent -> Opaque queue
 *    - No manual queue selection needed
 *
 * 3. Rendering Order (Critical):
 *    - First: Opaque (fill depth buffer)
 *    - Then: Transparent (use depth buffer for occlusion)
 *    - State management: IRenderer handles transitions
 *
 * 4. State Management (November 14 Refactor):
 *    - Before: Direct OpenGL calls (glDepthMask, glEnable)
 *    - After: IRenderer interface (renderer.beginOpaquePass, etc.)
 *    - Result: API-agnostic (GL/Vulkan support)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === SETUP (Per-Frame) ===
 * RenderQueueManager queueMgr;
 *
 * // === SUBMIT (Scene Traversal) ===
 * for (auto& obj : visibleObjects) {
 *     float distance = glm::length(camera.position - obj->transform.position);
 *
 *     // Automatic routing based on material.isTransparent
 *     queueMgr.submit(obj, obj->material, distance);
 * }
 *
 * // === SORT (Optimize Both Queues) ===
 * queueMgr.sort();
 *
 * // === RENDER (With State Management) ===
 * queueMgr.render(*shader, *renderer);  // IShader, IRenderer interfaces
 *
 * // === STATISTICS ===
 * LOG_INFO("Total: {} objects, {} binds, {} saved ({:.1f}% reduction)",
 *     queueMgr.getTotalCount(),
 *     queueMgr.getTotalMaterialBinds(),
 *     queueMgr.getTotalMaterialBindsSaved(),
 *     (queueMgr.getTotalMaterialBindsSaved() /
 *      float(queueMgr.getTotalCount())) * 100.0f
 * );
 *
 * // === CLEAR (Prepare for Next Frame) ===
 * queueMgr.clear();
 * ```
 *
 * AUTOMATIC ROUTING - How It Works:
 *
 * ```cpp
 * void RenderQueueManager::submit(SceneObject* object, const Material& material, float distance) {
 *     if (material.isTransparent) {
 *         // Glass, particles, water -> Transparent queue
 *         m_transparentQueue.submit(object, &material, distance);
 *     } else {
 *         // Solid objects -> Opaque queue
 *         m_opaqueQueue.submit(object, &material, distance);
 *     }
 * }
 * ```
 *
 * Result: No manual queue selection, automatic correctness
 *
 * RENDERING ORDER - Critical for Correctness:
 *
 * ```cpp
 * void RenderQueueManager::render(IShader& shader, IRenderer& renderer) {
 *     // === OPAQUE PASS ===
 *     // State: Depth write ON, blending OFF
 *     renderer.beginOpaquePass();
 *     m_opaqueQueue.execute(shader);
 *     renderer.endPass();
 *
 *     // === TRANSPARENT PASS ===
 *     // State: Depth write OFF, blending ON
 *     renderer.beginTransparentPass();
 *     m_transparentQueue.execute(shader);
 *     renderer.endPass();
 * }
 * ```
 *
 * Opaque pass (November 14 - IRenderer abstraction):
 * - renderer.beginOpaquePass(): Depth write ON, blending OFF
 * - Fill depth buffer (Z-buffer)
 * - Material batching (98% reduction)
 * - Early-Z optimization (GPU rejects occluded pixels)
 *
 * Transparent pass (November 14 - IRenderer abstraction):
 * - renderer.beginTransparentPass(): Depth write OFF, blending ON
 * - Use filled depth buffer (occlude by opaque)
 * - Distance sorting (back-to-front, correct blending)
 * - Some material batching (when distances close)
 *
 * STATE MANAGEMENT EVOLUTION:
 *
 * Before (October 26-28, 2025 - Direct OpenGL):
 * ```cpp
 * // Opaque pass
 * glDepthMask(GL_TRUE);   // Write depth
 * glDisable(GL_BLEND);    // No blending
 * m_opaqueQueue.execute(shader);
 *
 * // Transparent pass
 * glDepthMask(GL_FALSE);  // Don't write depth
 * glEnable(GL_BLEND);     // Enable blending
 * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 * m_transparentQueue.execute(shader);
 *
 * // Restore
 * glDepthMask(GL_TRUE);
 * glDisable(GL_BLEND);
 * ```
 * Problem: Direct OpenGL calls break API abstraction (Vulkan incompatible)
 *
 * After (November 14, 2025 - IRenderer abstraction):
 * ```cpp
 * // Opaque pass
 * renderer.beginOpaquePass();  // Depth ON, blend OFF (via IRenderer)
 * m_opaqueQueue.execute(shader);
 * renderer.endPass();
 *
 * // Transparent pass
 * renderer.beginTransparentPass();  // Depth OFF, blend ON (via IRenderer)
 * m_transparentQueue.execute(shader);
 * renderer.endPass();
 * ```
 * Benefit: API-agnostic (works with GLRenderer, future VKRenderer)
 *
 * STATISTICS - Test Scene:
 *
 * Scene composition:
 * - 371 objects total (351 opaque + 20 transparent)
 * - 7 unique materials (6 opaque + 1 transparent)
 *
 * Without batching (naive):
 * - Material binds: ~371 (one per object)
 * - Performance: ~35fps
 *
 * With batching (dual-queue):
 * - Opaque: 351 objects -> 6 material binds
 * - Transparent: 20 objects -> 1 material bind
 * - Total: 371 objects -> 7 material binds (364 saved, 98.1% reduction!)
 * - Performance: ~60fps (71% improvement)
 *
 * OPAQUE VS TRANSPARENT - Rendering Differences:
 *
 * Opaque objects:
 * - Order: Material-first (batching priority)
 * - Secondary: Front-to-back (early-Z optimization)
 * - Depth writes: ON (fill Z-buffer)
 * - Blending: OFF (solid rendering)
 * - GPU optimization: Early-Z rejection (reject occluded pixels)
 *
 * Transparent objects:
 * - Order: Distance-first (back-to-front, painter's algorithm)
 * - Secondary: Material (batch when distances close)
 * - Depth writes: OFF (don't block objects behind)
 * - Blending: ON (alpha compositing)
 * - Constraint: Order-dependent (blending not commutative)
 *
 * Why separate queues?
 * - Opaque: Any order works (depth test handles occlusion)
 * - Transparent: MUST be back-to-front (blending order matters)
 * - Solution: Different sorting strategies, separate queues
 *
 * CURRENT STATE (November 14, 2025):
 * - Dual-queue system (opaque + transparent)
 * - Automatic routing (based on material.isTransparent)
 * - Opaque front-to-back sorting (early-Z + batching)
 * - Transparent back-to-front sorting (correct blending)
 * - IRenderer state management (API-agnostic, November 14 refactor)
 * - 98%+ batching efficiency achieved
 * - Status: Production-ready, API-agnostic
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. Single Shader Per Frame:
 * - Assumes one shader for entire render
 * - Future: Multi-shader support 
 *
 * 2. Standard Alpha Blending Only:
 * - GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
 * - Future: Additive (particles), multiply (shadows)
 *
 * 3. Static Distance:
 * - Calculated once (not updated if camera moves)
 * - Acceptable: Camera rarely moves mid-frame
 *
 * 4. No Order-Independent Transparency:
 * - Painter's algorithm has limitations
 * - Future: Depth peeling, weighted blended OIT 
 *
 * 5. No Multi-Layer Transparency:
 * - Single transparent pass
 * - Future: Multiple passes for layered glass
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 26, 2025: Initial implementation
 * - Dual-queue system (opaque + transparent)
 * - Automatic routing based on transparency
 * - Direct OpenGL state management
 *
 * October 26-28, 2025: Testing + Fixes
 * - Test scene with 371 objects
 * - 98.1% reduction achieved
 * - Statistics tracking
 *
 * November 6-14, 2025: Interface Abstraction
 * - IShader interface integration (not Shader*)
 * - IRenderer interface integration (state management)
 * - Removed direct OpenGL calls
 * - API-agnostic (GL/Vulkan support)
 * - Status: Complete, production-ready
 *
 * (Multi-Shader Support):
 * - Sort: shader -> material -> mesh -> distance
 * - Further optimization
 * - Time: 2-3 days
 *
 * (Additional Blend Modes):
 * - Additive blending (particles, fire)
 * - Multiply blending (shadows, fog)
 * - Premultiplied alpha (correct blending)
 * - Time: 1-2 days
 *
 * (Order-Independent Transparency):
 * - Depth peeling (multi-pass)
 * - Weighted blended OIT (single-pass)
 * - Handles complex transparent scenes
 * - Time: 1 week
 *
 * DEPENDENCIES:
 * - renderer/RenderQueue.h: Queue implementation (98% reduction)
 * - renderer/interface/IShader.h: Shader interface (GL/Vulkan)
 * - renderer/interface/IRenderer.h: State management interface (November 14)
 * - core/Logger.h: Statistics logging
 *
 * THREAD SAFETY:
 * - NOT thread-safe: No mutex protection
 * - Single-threaded usage only (main render thread)
 * - Future: Lock-free multi-threaded submission
 *
 * REFERENCES:
 * - Game Engine Architecture 3rd Ed., Chapter 10: Rendering optimization
 * - Real-Time Rendering 4th Ed., Chapter 5: Visual appearance (transparency)
 * - DOOM 2016 GDC: Rendering architecture (dual-queue system)
 *
 * HISTORY:
 * October 26, 2025: Initial implementation
 * - Dual-queue system (opaque + transparent)
 * - Automatic routing based on material transparency
 * - Direct OpenGL state management (glDepthMask, glEnable)
 *
 * October 26-28, 2025: Testing + Validation
 * - Test scene: 371 objects, 7 materials
 * - Result: 98.1% reduction (7 binds vs 371 naive)
 * - Statistics tracking implemented
 *
 * November 6-14, 2025: Interface Abstraction Refactor
 * - Replaced Shader* with IShader& (polymorphic)
 * - Integrated IRenderer for state management (API-agnostic)
 * - Removed direct OpenGL calls (glDepthMask, glEnable, etc.)
 * - Added renderer.beginOpaquePass(), renderer.beginTransparentPass()
 * - Result: API-agnostic dual-queue system (GL/Vulkan ready)
 *
 * November 24-25, 2025: CRITICAL BUG FIX - Memory Leak (Push/Pop Mismatch)
 * - Bug: Missing renderer.endPass() after opaque rendering pass
 * - Symptom: Linear memory leak 0.2 MB/sec (7+ GB over 20 hours)
 * - Root Cause: beginOpaquePass() pushed state, but no corresponding endPass()
 * - Only transparent pass was properly balanced (begin + end)
 * - Opaque state leaked every frame (stack depth grew unbounded)
 * - Discovery: 20-hour soak test + VS Memory Profiler (snapshot comparison)
 * - Investigation: 6 hours (profiling, isolation testing, stack trace analysis)
 * - Fix: Added missing renderer.endPass() after m_opaqueQueue.execute() (1 line)
 * - Irony: Header documentation correctly showed endPass() usage in example code
 * - Implementation failed to match documentation (overlooked during refactor)
 * - Lesson: Push/pop balance critical - every begin*Pass() needs matching endPass()
 * - Verification: 10-minute profiler test confirmed zero leak (memory stable at 52 MB)
 * - Result: Production-quality renderer with zero memory leaks
 */

namespace Engine
{
    // Forward declarations
    class SceneObject;
    class Material;

    class RenderQueueManager
    {
    public:
        RenderQueueManager() = default;
        ~RenderQueueManager() = default;

        // Submit object to appropriate queue based on material transparency
        void submit(SceneObject* object, const Material& material, float distance);

        // Sort both queues (opaque front-to-back, transparent back-to-front)
        void sort();

        // Render both queues in correct order with state management
        void render(IShader& shader, IRenderer& renderer);  

        // Clear both queues for next frame
        void clear();

        // Statistics - Opaque Queue
        size_t getOpaqueCount() const { return m_opaqueQueue.getCommandCount(); }
        int getOpaqueMaterialBinds() const { return m_opaqueQueue.getMaterialBindCount(); }
        int getOpaqueMaterialBindsSaved() const { return m_opaqueQueue.getMaterialBindsSaved(); }

        // Statistics - Transparent Queue
        size_t getTransparentCount() const { return m_transparentQueue.getCommandCount(); }
        int getTransparentMaterialBinds() const { return m_transparentQueue.getMaterialBindCount(); }
        int getTransparentMaterialBindsSaved() const { return m_transparentQueue.getMaterialBindsSaved(); }

        // Statistics - Combined
        size_t getTotalCount() const { return getOpaqueCount() + getTransparentCount(); }
        int getTotalMaterialBinds() const { return getOpaqueMaterialBinds() + getTransparentMaterialBinds(); }
        int getTotalMaterialBindsSaved() const { return getOpaqueMaterialBindsSaved() + getTransparentMaterialBindsSaved(); }

    private:
        RenderQueue m_opaqueQueue;       // Solid objects (front-to-back, material-first)
        RenderQueue m_transparentQueue;  // See-through objects (back-to-front, distance-first)
    };
}