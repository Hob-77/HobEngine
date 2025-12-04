#pragma once
#include "renderer/interface/ITexture.h"
#include <glad/glad.h>
#include <string>

/*
 * GLTexture.h
 *
 * PURPOSE:
 * OpenGL 2D texture implementation. Loads images via stb_image, uploads to GPU, generates
 * mipmaps, and applies anisotropic filtering. Implements ITexture interface for renderer
 * abstraction. Provides fallback magenta checkerboard on load failure (no crashes, easy
 * debugging). Supports configurable wrapping, filtering, and anisotropic filtering for
 * high-quality texture sampling.
 *
 * DESIGN RATIONALE (November 6, 2025):
 * Problem: Need concrete OpenGL texture implementation. Image loading is error-prone (missing
 * files, corrupt data). Textures at oblique angles (floors, walls) appear blurry without
 * anisotropic filtering. Need flexible parameter system for different texture types (pixel
 * art, diffuse maps, normal maps).
 *
 * Solution: RAII wrapper around OpenGL textures with stb_image loader and fallback system.
 * - Image loading: stb_image handles PNG/JPG/BMP/TGA formats
 * - Fallback: Magenta checkerboard on failure (highly visible, no crash)
 * - Anisotropic filtering: Hardware-accelerated quality boost (nearly free)
 * - Mipmaps: Auto-generated for anti-aliasing and performance
 * - Parameters: Flexible wrapping/filtering per texture
 *
 * Key Insight: Anisotropic filtering is nearly free on modern GPUs but provides massive quality
 * improvement for textures viewed at oblique angles. 16× AF is standard in AAA games. Fallback
 * textures prevent crashes and make missing assets immediately obvious (bright magenta).
 *
 * DESIGN PHILOSOPHY:
 * - Fail gracefully: Missing files -> fallback texture (no crash)
 * - Quality by default: Mipmaps + anisotropic filtering enabled
 * - RAII: Constructor loads, destructor deletes GPU texture
 * - Move-only: Prevent GPU resource duplication
 * - Configurable: Parameters struct for custom behavior
 *
 * KEY CONCEPTS:
 * 1. Texture Basics:
 *    - 2D image stored in GPU memory
 *    - Sampled in fragment shader via UV coordinates (0,0 -> 1,1)
 *    - GPU interpolates between pixels (filtering)
 *
 * 2. Anisotropic Filtering (AF):
 *    - Improves texture quality at oblique angles (floors, walls)
 *    - Without AF: Blurry/muddy at distance
 *    - With AF 16×: Sharp and detailed even at grazing angles
 *    - Performance: Nearly free (~0ms overhead on modern GPUs)
 *    - Requires mipmaps
 *
 * 3. Mipmaps:
 *    - Pre-generated smaller versions (1/2, 1/4, 1/8, etc. size)
 *    - Prevents aliasing (flickering) at distance
 *    - Improves performance (smaller texture = faster sampling)
 *    - Generated automatically via glGenerateMipmap
 *
 * 4. Fallback System:
 *    - Missing files -> 2×2 magenta checkerboard (highly visible)
 *    - isFallback() query detects failed loads
 *    - Engine continues running (no crash)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === RECOMMENDED (Via AssetManager) ===
 * // Automatically applies global anisotropic filtering
 * auto diffuse = AssetManager::get().loadTexture("textures/crate.jpg");
 * diffuse->bind(0);
 * shader->setUniform("u_DiffuseMap", 0);
 *
 * // === CONFIGURE GLOBAL AF (Application Startup) ===
 * Application({
 *     .render = {
 *         .anisotropicFiltering = 16  // Max quality (default)
 *     }
 * });
 * // All textures use 16× AF automatically
 *
 * // === CUSTOM PARAMETERS (Pixel Art) ===
 * GLTexture::Parameters pixelArt;
 * pixelArt.minFilter = TextureFilter::Nearest;    // Sharp, no blur
 * pixelArt.magFilter = TextureFilter::Nearest;    // Sharp, no blur
 * pixelArt.generateMipmaps = false;               // Mipmaps blur pixel art
 * pixelArt.anisotropy = 0;                        // AF not needed
 *
 * auto sprite = std::make_shared<GLTexture>("sprites/block.png", pixelArt);
 *
 * // === CUSTOM PARAMETERS (Tiling Texture) ===
 * GLTexture::Parameters tiling;
 * tiling.wrapS = TextureWrap::Repeat;             // Tile horizontally
 * tiling.wrapT = TextureWrap::Repeat;             // Tile vertically
 * tiling.minFilter = TextureFilter::LinearMipmapLinear;
 * tiling.generateMipmaps = true;
 * tiling.anisotropy = 16;                         // Max quality for floors
 *
 * auto floor = std::make_shared<GLTexture>("textures/floor.jpg", tiling);
 *
 * // === CHECK FOR FALLBACK (Validation) ===
 * auto texture = AssetManager::get().loadTexture("missing.png");
 * if (texture->isFallback()) {
 *     LOG_WARN("Failed to load texture, using fallback");
 * }
 * ```
 *
 * TEXTURE PARAMETERS:
 *
 * Wrapping modes (behavior when UV > 1.0):
 * ```cpp
 * enum class TextureWrap {
 *     Repeat,          // Texture tiles (default for seamless textures)
 *     ClampToEdge,     // Edge pixels stretch (prevents seams for UI)
 *     MirroredRepeat,  // Texture mirrors (seamless tiling without visible seams)
 *     ClampToBorder    // Clamp to border color (custom color outside UV range)
 * };
 * ```
 *
 * Filtering modes (sampling method):
 * ```cpp
 * enum class TextureFilter {
 *     Nearest,                // Sharp, pixelated (pixel art, retro games)
 *     Linear,                 // Smooth, blurred (general purpose)
 *     NearestMipmapNearest,   // Mipmap, sharp (fast, low quality)
 *     LinearMipmapNearest,    // Mipmap, medium (balanced)
 *     NearestMipmapLinear,    // Mipmap, medium (balanced)
 *     LinearMipmapLinear      // Mipmap, smooth (default, best quality)
 * };
 * ```
 *
 * Default parameters:
 * ```cpp
 * struct Parameters {
 *     TextureWrap wrapS = TextureWrap::Repeat;                  // Horizontal wrap
 *     TextureWrap wrapT = TextureWrap::Repeat;                  // Vertical wrap
 *     TextureFilter minFilter = TextureFilter::LinearMipmapLinear;  // Minification
 *     TextureFilter magFilter = TextureFilter::Linear;          // Magnification
 *     bool generateMipmaps = true;                              // Auto mipmaps
 *     int anisotropy = 0;  // 0=off, 2/4/8/16=quality level (set by AssetManager)
 * };
 * ```
 *
 * ANISOTROPIC FILTERING - Technical Details:
 *
 * What it does:
 * - Standard filtering: Circular sampling footprint (isotropic)
 * - Anisotropic filtering: Elliptical sampling footprint (follows surface angle)
 * - Result: Sharper textures at oblique angles (floors, walls, distant surfaces)
 *
 * Performance:
 * - Hardware accelerated on all modern GPUs (2005+)
 * - Cost: ~0.01ms for 1920×1080 at 16× AF (negligible)
 * - Memory: No additional VRAM (uses existing mipmaps)
 *
 * Quality levels:
 * - 0× (disabled): Standard filtering (blurry at angles)
 * - 2×: Slight improvement
 * - 4×: Noticeable improvement
 * - 8×: Significant improvement
 * - 16×: Maximum quality (industry standard)
 *
 * Implementation:
 * ```cpp
 * if (params.anisotropy > 0 && params.generateMipmaps) {
 *     // Query max supported AF level
 *     float maxAnisotropy;
 *     glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
 *
 *     // Clamp requested AF to hardware max
 *     float af = std::min(static_cast<float>(params.anisotropy), maxAnisotropy);
 *
 *     // Apply AF (requires mipmaps)
 *     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, af);
 * }
 * ```
 *
 * MIPMAP GENERATION:
 *
 * ```cpp
 * if (params.generateMipmaps) {
 *     glGenerateMipmap(GL_TEXTURE_2D);
 *     // Generates levels: 1024×1024 -> 512×512 -> 256×256 -> ... -> 1×1
 * }
 * ```
 *
 * Benefits:
 * - Anti-aliasing: Prevents flickering at distance (texture aliasing)
 * - Performance: Smaller textures faster to sample (cache efficiency)
 * - Quality: Reduces moiré patterns and temporal aliasing
 * - Required for: Anisotropic filtering, mipmap filtering modes
 *
 * FALLBACK TEXTURE:
 *
 * ```cpp
 * void GLTexture::createFallbackTexture(const Parameters& params) {
 *     // 2×2 magenta checkerboard (highly visible)
 *     unsigned char fallbackData[] = {
 *         255, 0, 255, 255,    // Magenta
 *         0, 0, 0, 255,        // Black
 *         0, 0, 0, 255,        // Black
 *         255, 0, 255, 255     // Magenta
 *     };
 *
 *     // Upload to GPU
 *     glGenTextures(1, &m_textureID);
 *     glBindTexture(GL_TEXTURE_2D, m_textureID);
 *     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
 *                  GL_UNSIGNED_BYTE, fallbackData);
 *
 *     // Apply parameters
 *     applyParameters(params);
 *
 *     m_width = 2;
 *     m_height = 2;
 *     m_channels = 4;
 *     m_isFallback = true;
 * }
 * ```
 *
 * Why magenta checkerboard?
 * - Highly visible: Stands out in any scene (debug-friendly)
 * - Not confusable: No natural material is bright magenta
 * - Industry standard: Unity, Unreal, Source Engine all use magenta
 * - Easy to spot: Artists/developers immediately see missing textures
 *
 * RESOURCE MANAGEMENT:
 *
 * ```cpp
 * class GLTexture {
 * public:
 *     // Constructor: Load and upload to GPU
 *     GLTexture(const char* filepath, const Parameters& params) {
 *         // Load image via stb_image
 *         unsigned char* data = stbi_load(filepath, &m_width, &m_height, &m_channels, 0);
 *         if (!data) {
 *             LOG_ERROR("Failed to load texture: {}", filepath);
 *             createFallbackTexture(params);
 *             return;
 *         }
 *
 *         // Create OpenGL texture
 *         glGenTextures(1, &m_textureID);
 *         glBindTexture(GL_TEXTURE_2D, m_textureID);
 *
 *         // Upload to GPU
 *         GLenum format = (m_channels == 4) ? GL_RGBA : GL_RGB;
 *         glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0,
 *                      format, GL_UNSIGNED_BYTE, data);
 *
 *         // Apply parameters
 *         applyParameters(params);
 *
 *         // Free CPU memory
 *         stbi_image_free(data);
 *     }
 *
 *     // Destructor: Delete GPU texture
 *     ~GLTexture() {
 *         if (m_textureID != 0) {
 *             glDeleteTextures(1, &m_textureID);
 *         }
 *     }
 *
 *     // Move semantics: Transfer ownership
 *     GLTexture(GLTexture&& other) noexcept
 *         : m_textureID(other.m_textureID)
 *         , m_width(other.m_width)
 *         , m_height(other.m_height)
 *         , m_channels(other.m_channels)
 *         , m_filepath(std::move(other.m_filepath))
 *         , m_isFallback(other.m_isFallback)
 *     {
 *         other.m_textureID = 0;  // Prevent double-delete
 *     }
 *
 *     // Copy deleted: Prevent GPU resource duplication
 *     GLTexture(const GLTexture&) = delete;
 * };
 * ```
 *
 * GLTEXTUREVIEW (Non-Owning Wrapper):
 *
 * Purpose: Wrap framebuffer textures without owning them
 * ```cpp
 * class GLTextureView : public ITexture {
 *     // Non-owning reference to FBO color attachment
 *     GLuint m_textureID;  // Does NOT delete in destructor
 * };
 * ```
 *
 * Use case:
 * - Framebuffer creates color attachment (owns texture)
 * - GLTextureView wraps attachment for Material/Shader binding
 * - Framebuffer destructor deletes texture (View doesn't)
 * - Result: Clean abstraction without ownership ambiguity
 *
 * CURRENT STATE (November 6, 2025):
 * - 2D texture support (PNG, JPG, BMP, TGA)
 * - Anisotropic filtering (configurable, default 16×)
 * - Automatic mipmap generation
 * - Fallback magenta checkerboard on failure
 * - Flexible parameters (wrapping, filtering)
 * - RAII resource management, move-only semantics
 * - GLTextureView for framebuffer attachments
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. 2D Textures Only:
 * - No cubemaps (skybox, reflections)
 * - No 3D textures (volumetric effects)
 * - No texture arrays (terrain splatting)
 * - Future: (cubemaps), (3D/arrays)
 *
 * 2. No Compressed Formats:
 * - No DXT/BC compression (save VRAM)
 * - No ASTC (mobile optimization)
 * - Future: (compression support)
 *
 * 3. No HDR Loading:
 * - No .hdr, .exr formats (IBL skyboxes)
 * - Future: (IBL implementation)
 *
 * 4. AF Applied at Load Time:
 * - Can't adjust AF at runtime (graphics settings)
 * - Would require texture reload
 * - Future: (runtime adjustable)
 *
 * 5. No Asynchronous Loading:
 * - Loads on main thread (blocks rendering)
 * - Future: (background loading)
 *
 * INTEGRATION WITH ROADMAP:
 *
 * November 6, 2025: Initial implementation
 * - OpenGL 2D texture wrapper (glGenTextures, glTexImage2D)
 * - stb_image integration (PNG/JPG/BMP/TGA loading)
 * - Anisotropic filtering support (configurable quality)
 * - Fallback magenta checkerboard
 * - RAII resource management, move-only semantics
 * - ITexture interface implementation
 *
 * (Cubemap Support):
 * - GLTextureCubemap class (skybox, reflections)
 * - 6-face loading (±X, ±Y, ±Z)
 * - Time: Already implemented 
 *
 * (HDR Loading):
 * - .hdr, .exr format support (IBL skyboxes)
 * - GL_RGB16F internal format (HDR framebuffers)
 * - Time: 1-2 days
 *
 * (Advanced Features):
 * - Texture compression (DXT/BC/ASTC)
 * - Asynchronous loading (background thread)
 * - Runtime AF adjustment (graphics settings)
 * - Texture streaming (load high-res on demand)
 * - Time: 2-3 weeks total
 *
 * DEPENDENCIES:
 * - renderer/interface/ITexture.h: Abstract interface
 * - <glad/glad.h>: OpenGL function loader
 * - stb_image.h: Image loading library
 * - <string>: Filepath storage
 *
 * THREAD SAFETY:
 * - NOT thread-safe: OpenGL context requirement
 * - Loading: Main thread only (stbi_load, glTexImage2D)
 * - Binding: Main thread only (glBindTexture)
 * - Future: Async loading possible (stbi_load on thread, upload on main)
 *
 * REFERENCES:
 * - OpenGL 4.6 Specification: Texture objects, anisotropic filtering
 * - stb_image documentation: Image loading
 * - LearnOpenGL.com: Texture tutorial
 * - ITexture.h: Interface documentation
 *
 * HISTORY:
 * October 6, 2025: Original implementation
 * - OpenGL basic texture creation from LearnOpenGL
 * - This was a basic implementation allowing us to test a triangle and cube
 * 
 * November 6, 2025: Initial implementation
 * - OpenGL 2D texture wrapper (stb_image + glTexImage2D)
 * - Anisotropic filtering support (GL_TEXTURE_MAX_ANISOTROPY)
 * - Automatic mipmap generation (glGenerateMipmap)
 * - Fallback magenta checkerboard on load failure
 * - Flexible Parameters struct (wrapping, filtering, AF)
 * - RAII resource management, move-only semantics
 * - GLTextureView for framebuffer attachments
 * - Result: High-quality texture system with AF
 *
 */

namespace Engine
{
	enum class TextureWrap
	{
		Repeat,
		ClampToEdge,
		MirroredRepeat,
		ClampToBorder
	};

	enum class TextureFilter
	{
		Nearest,
		Linear,
		NearestMipmapNearest,
		LinearMipmapNearest,
		NearestMipmapLinear,
		LinearMipmapLinear
	};

	class GLTexture : public ITexture
	{
	public:
		struct Parameters
		{
			TextureWrap wrapS = TextureWrap::Repeat;
			TextureWrap wrapT = TextureWrap::Repeat;
			TextureFilter minFilter = TextureFilter::LinearMipmapLinear;
			TextureFilter magFilter = TextureFilter::Linear;
			bool generateMipmaps = true;
			int anisotropy = 0;  // 0 = disabled, 2/4/8/16 = enabled
		};

		// Constructors
		GLTexture(const char* filepath);  // Use default parameters
		GLTexture(const char* filepath, const Parameters& params);
		~GLTexture() override;

		// Move semantics (no copying)
		GLTexture(GLTexture&& other) noexcept;
		GLTexture& operator=(GLTexture&& other) noexcept;
		GLTexture(const GLTexture&) = delete;
		GLTexture& operator=(const GLTexture&) = delete;

		// ITexture interface implementation
		void bind(uint32_t slot = 0) const override;
		void unbind() const override;

		uint32_t getWidth() const override { return m_width; }
		uint32_t getHeight() const override { return m_height; }
		uint32_t getChannels() const override { return m_channels; }
		bool isValid() const override { return m_textureID != 0; }
		bool isFallback() const override { return m_isFallback; }

		// OpenGL-specific query (not in interface)
		GLuint getTextureID() const { return m_textureID; }
		const std::string& getFilepath() const { return m_filepath; }

	private:
		void createFallbackTexture(const Parameters& params);

	private:
		GLuint m_textureID = 0;
		int m_width = 0;
		int m_height = 0;
		int m_channels = 0;
		std::string m_filepath;
		bool m_isFallback = false;
	};

	class GLTextureView : public ITexture
	{
	public:
		GLTextureView(GLuint textureID, int width, int height, int channels)
			: m_textureID(textureID), m_width(width), m_height(height), m_channels(channels)
		{
		}

		// ITexture interface
		void bind(uint32_t slot = 0) const override
		{
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(GL_TEXTURE_2D, m_textureID);
		}

		void unbind() const override
		{
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		uint32_t getWidth() const override { return m_width; }
		uint32_t getHeight() const override { return m_height; }
		uint32_t getChannels() const override { return m_channels; }
		bool isValid() const override { return m_textureID != 0; }
		bool isFallback() const override { return false; }

	private:
		GLuint m_textureID;  // Non-owning reference to FBO's texture
		int m_width, m_height, m_channels;
	};

}