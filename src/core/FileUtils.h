#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "core/Logger.h"
#include "core/Error.h"

/*
 * FileUtils.h
 *
 * PURPOSE:
 * Simple, synchronous file I/O helpers for asset loading and text processing. Provides
 * basic read operations for shaders, models, textures, and configuration files. Foundation
 * for AssetManager and resource loading pipeline. Minimal, grows as needed (YAGNI).
 *
 * DESIGN RATIONALE (October 6, 2025):
 * Problem: Need to load files (shaders, textures, models, configs). std::ifstream verbose
 * (boilerplate in every loader). Need consistent error handling (log + assert). Need both
 * text and binary modes (shaders vs images).
 *
 * Solution: Static utility class with simple file reading functions.
 * - readFile(): Text mode (shaders, JSON, XML)
 * - readBinary(): Binary mode (images, models, audio)
 * - Error handling: Log + assert (fail fast in debug)
 * - Blocking I/O: Simple, sufficient for development
 * - Result: Clean API, no boilerplate
 *
 * Key Insight: File I/O fundamental to engines (load everything from disk). Static utility
 * pattern natural (no state, global access). Fail-fast philosophy (missing assets = broken
 * game, crash immediately in debug). Text vs binary modes critical (line ending conversion
 * breaks binary files). Whole-file loading simpler than streaming (sufficient for assets).
 * Grew organically (added features as needed for hot-reload, OBJ loader).
 *
 * DESIGN PHILOSOPHY:
 * - Static utilities: No state, no instances
 * - Blocking I/O: Simple, predictable (sufficient for dev)
 * - Fail-fast: Assert on error (loud failures in debug)
 * - Whole-file loading: Simpler than streaming
 * - Grow as needed: YAGNI (add features when required)
 *
 * KEY CONCEPTS:
 * 1. Text Mode (readFile):
 *    - Line ending conversion (CRLF -> LF on Windows)
 *    - Character encoding aware (UTF-8, ASCII)
 *    - Stops at EOF marker
 *    - Use for: Shaders, JSON, XML, TXT
 *
 * 2. Binary Mode (readBinary):
 *    - Raw byte stream (no conversion)
 *    - Exact contents preserved (lossless)
 *    - Reads complete file (including nulls)
 *    - Use for: Images (PNG, JPG), models (OBJ), audio (WAV)
 *
 * 3. Error Handling (Fail-Fast):
 *    - File not found: LOG_ERROR + ENGINE_ASSERT (crash debug)
 *    - Read failure: LOG_ERROR + ENGINE_ASSERT (crash debug)
 *    - Returns empty (never reached due to assert)
 *
 * 4. Performance:
 *    - Synchronous (blocks until loaded)
 *    - Whole-file (loads entire file at once)
 *    - Fast for small files (< 10MB)
 *    - May stutter for large files (> 100MB)
 *
 * USAGE EXAMPLE:
 * ```cpp
 * // === READ SHADER (TEXT MODE) ===
 * std::string vertexSource = FileUtils::readFile("assets/shaders/basic.vert");
 * std::string fragmentSource = FileUtils::readFile("assets/shaders/basic.frag");
 *
 * Shader shader(vertexSource.c_str(), fragmentSource.c_str());
 *
 * // === READ TEXTURE (BINARY MODE) ===
 * std::vector<uint8_t> imageData = FileUtils::readBinary("assets/textures/wood.png");
 *
 * // Pass to stb_image
 * int width, height, channels;
 * unsigned char* pixels = stbi_load_from_memory(imageData.data(), imageData.size(),
 *                                                &width, &height, &channels, 4);
 *
 * // === READ MODEL (BINARY MODE) ===
 * std::vector<uint8_t> modelData = FileUtils::readBinary("assets/models/cube.obj");
 *
 * // Pass to OBJ loader
 * OBJLoader loader;
 * Mesh mesh = loader.load(modelData);
 *
 * // === READ CONFIG (TEXT MODE) ===
 * std::string configJson = FileUtils::readFile("assets/config.json");
 *
 * // Parse JSON
 * auto config = JSON::parse(configJson);
 * ```
 *
 * TEXT MODE vs BINARY MODE - When to Use Each:
 *
 * Text mode (readFile):
 * - Shaders: .vert, .frag, .geom, .comp
 * - Configs: .json, .xml, .yaml, .ini
 * - Text files: .txt, .md
 * - Line endings: May convert CRLF -> LF (platform-specific)
 * - Encoding: UTF-8, ASCII
 * - Stop: EOF marker
 *
 * Binary mode (readBinary):
 * - Images: .png, .jpg, .bmp, .tga
 * - Models: .obj, .fbx, .gltf (binary)
 * - Audio: .wav, .mp3, .ogg
 * - Data: .bin, .dat
 * - Exact: No conversion, lossless
 * - Complete: Reads entire file (including null bytes)
 *
 * Rule of thumb: When in doubt, use binary mode (safer, lossless)
 *
 * ERROR HANDLING - Fail-Fast Philosophy:
 *
 * ```cpp
 * std::string readFile(const char* filepath) {
 *     std::ifstream file(filepath);
 *
 *     if (!file.is_open()) {
 *         LOG_ERROR("Failed to open file: {}", filepath);
 *         ENGINE_ASSERT(false, "File not found");
 *         return "";  // Never reached (assert crashes in debug)
 *     }
 *
 *     // ... read file
 * }
 * ```
 *
 * Why crash on failure?
 * - Missing assets = broken game (unplayable)
 * - Silent failures hide bugs (hard to debug)
 * - Loud failures force immediate fix (better DX)
 * - Development philosophy: Fix root cause, not symptoms
 *
 * Production consideration:
 * - Release builds: Assertions compiled out (ENGINE_ASSERT -> nothing)
 * - Need graceful fallback (return empty, use default texture)
 * - Future: Add release-safe error handling 
 *
 * IMPLEMENTATION DETAILS:
 *
 * readFile (text mode):
 * ```cpp
 * std::string readFile(const char* filepath) {
 *     // 1. Open file (text mode, default)
 *     std::ifstream file(filepath);
 *
 *     if (!file.is_open()) {
 *         // Error handling (log + assert)
 *         LOG_ERROR("Failed to open file: {}", filepath);
 *         ENGINE_ASSERT(false, "File not found");
 *         return "";
 *     }
 *
 *     // 2. Read entire file via rdbuf() -> stringstream
 *     std::stringstream buffer;
 *     buffer << file.rdbuf();  // Efficient, reads entire file
 *
 *     // 3. Close file
 *     file.close();
 *
 *     // 4. Return as string
 *     return buffer.str();
 * }
 * ```
 *
 * readBinary (binary mode):
 * ```cpp
 * std::vector<uint8_t> readBinary(const char* filepath) {
 *     // 1. Open file (binary mode, seek to end)
 *     std::ifstream file(filepath, std::ios::binary | std::ios::ate);
 *     // std::ios::ate: Seek to end (get size)
 *
 *     if (!file.is_open()) {
 *         LOG_ERROR("Failed to open binary file: {}", filepath);
 *         ENGINE_ASSERT(false, "Binary file not found");
 *         return {};
 *     }
 *
 *     // 2. Get file size (current position = end)
 *     std::streamsize size = file.tellg();
 *
 *     // 3. Seek back to beginning
 *     file.seekg(0, std::ios::beg);
 *
 *     // 4. Allocate vector of exact size
 *     std::vector<uint8_t> buffer(size);
 *
 *     // 5. Read entire file into vector
 *     if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
 *         LOG_ERROR("Failed to read binary file: {}", filepath);
 *         ENGINE_ASSERT(false, "Binary file read failed");
 *         return {};
 *     }
 *
 *     // 6. Close and return
 *     file.close();
 *     LOG_INFO("Read {} bytes from binary file: {}", size, filepath);
 *     return buffer;
 * }
 * ```
 *
 * PERFORMANCE CONSIDERATIONS:
 *
 * Synchronous (blocking):
 * - Blocks main thread until file loaded
 * - Acceptable for small files (< 10MB, < 100ms)
 * - Problematic for large files (> 100MB, > 1s)
 * - May cause frame stutter during loading
 *
 * Whole-file loading:
 * - Loads entire file at once (not streaming)
 * - Simple API (no partial reads)
 * - Memory spike: File size in RAM
 *
 * Typical load times (SSD):
 * - 1KB shader: < 0.01ms (negligible)
 * - 1MB texture: ~1-5ms (acceptable)
 * - 10MB model: ~10-50ms (noticeable)
 * - 100MB level: ~100-500ms (unacceptable, need async)
 *
 * MEMORY USAGE:
 *
 * Peak memory: File size x 2
 * - Read buffer: std::ifstream internal buffer
 * - Return container: std::string or std::vector
 * - Example: 10MB file = ~20MB peak memory
 *
 * Cleanup:
 * - Automatic: std::string/vector manage memory
 * - RAII: Destructor frees memory
 * - No manual delete needed
 *
 * TYPICAL USE CASES:
 *
 * Shader loading:
 * ```cpp
 * std::string vert = FileUtils::readFile("basic.vert");
 * std::string frag = FileUtils::readFile("basic.frag");
 * auto shader = renderDevice->createShaderFromSource(vert, frag);
 * ```
 *
 * Texture loading:
 * ```cpp
 * std::vector<uint8_t> data = FileUtils::readBinary("wood.png");
 * int w, h, ch;
 * unsigned char* pixels = stbi_load_from_memory(data.data(), data.size(),
 *                                                &w, &h, &ch, 4);
 * auto texture = renderDevice->createTextureFromPixels(pixels, w, h);
 * ```
 *
 * Model loading:
 * ```cpp
 * std::vector<uint8_t> data = FileUtils::readBinary("cube.obj");
 * Mesh mesh = OBJLoader::load(data);
 * ```
 *
 * Config loading:
 * ```cpp
 * std::string json = FileUtils::readFile("settings.json");
 * Settings settings = JSON::parse(json);
 * ```
 *
 * CURRENT STATE (October 6, 2025+):
 * - Static utility class (no state, global access)
 * - readFile(): Text mode (shaders, configs)
 * - readBinary(): Binary mode (images, models, audio)
 * - Error handling: LOG_ERROR + ENGINE_ASSERT (fail-fast)
 * - Blocking I/O: Synchronous (simple, sufficient for dev)
 * - Whole-file loading: Loads entire file at once
 * - Grew organically: Added features as needed
 * - Status: Production-ready for small-medium files
 *
 * CURRENT LIMITATIONS (By Design):
 *
 * 1. No File Writing:
 * - Can't save configs, screenshots, save files
 * - Future: writeFile(), writeBinary()
 *
 * 2. No Existence Check:
 * - Must try-catch or handle assert
 * - Future: fileExists(path)
 *
 * 3. No Directory Operations:
 * - Can't list files, check if dir exists
 * - Future: listFiles(), directoryExists() 
 *
 * 4. No Path Manipulation:
 * - Can't get extension, directory, filename
 * - Future: getExtension(), getDirectory()
 *
 * 5. No File Watching:
 * - Can't detect changes (hot-reload manual)
 * - Future: FileWatcher class
 *
 * 6. No Async Loading:
 * - Blocks main thread (frame stutter)
 * - Future: AsyncFileLoader 
 *
 * 7. No Streaming:
 * - Can't read large files in chunks
 * - Future: FileStream class
 *
 * 8. No Compression:
 * - Can't read .zip, .gz files
 * - Future: Archive support
 *
 * INTEGRATION WITH ROADMAP:
 *
 * October 6, 2025: Initial implementation
 * - readFile() for text (shaders)
 * - readBinary() for binary (images, models)
 * - Fail-fast error handling
 * - Status: Basic functionality
 *
 * Ongoing: Organic growth
 * - Added features as needed
 * - Hot-reload support (shader reloading)
 * - OBJ loader integration (model loading)
 * - JSON config support (settings)
 *
 * (File Existence):
 * - bool fileExists(const char* path)
 * - Prevents assert crashes (check before load)
 * - Time: 1 hour
 *
 * (File Writing):
 * - writeFile(), writeBinary()
 * - Save configs, screenshots, save files
 * - Time: 2-3 hours
 *
 * (Path Manipulation):
 * - getExtension(), getDirectory(), getFilename()
 * - joinPath() for cross-platform paths
 * - Time: 2-3 hours
 *
 * (Directory Operations):
 * - listFiles(), directoryExists(), createDirectory()
 * - Asset enumeration, folder creation
 * - Time: 1 day
 *
 * (File Watching):
 * - FileWatcher class (hot-reload)
 * - Detect file changes, trigger reload
 * - Time: 1-2 days
 *
 * (Async Loading):
 * - AsyncFileLoader class (non-blocking)
 * - Background thread, callbacks
 * - Time: 2-3 days
 *
 * (Streaming):
 * - FileStream class (large files)
 * - Read in chunks (reduce memory)
 * - Time: 2-3 days
 *
 * DEPENDENCIES:
 * - <string>: std::string (text files)
 * - <fstream>: std::ifstream (file I/O)
 * - <sstream>: std::stringstream (text buffering)
 * - <vector>: std::vector<uint8_t> (binary data)
 * - core/Logger.h: LOG_ERROR, LOG_INFO
 * - core/Error.h: ENGINE_ASSERT
 *
 * THREAD SAFETY:
 * - Thread-safe: Static functions, no shared state
 * - Each call independent (no side effects)
 * - Can call from any thread (though typically main thread)
 *
 * REFERENCES:
 * - C++ std::ifstream documentation
 * - Game Engine Architecture 3rd Ed.: File I/O
 *
 * HISTORY:
 * October 6, 2025: Initial implementation
 * - Static FileUtils class
 * - readFile() for text mode (shaders, JSON, XML)
 * - readBinary() for binary mode (images, models, audio)
 * - Error handling: LOG_ERROR + ENGINE_ASSERT (fail-fast)
 * - Blocking I/O (synchronous, simple)
 * - Whole-file loading (not streaming)
 *
 * Ongoing: Organic growth
 * - Added features as needed (YAGNI philosophy)
 * - Hot-reload support (shader reloading, manual trigger)
 * - OBJ loader integration (model loading via readBinary)
 * - JSON config support (settings, levels)
 * - No major refactors (simple, stable)
 *
 */

namespace Engine
{
	class FileUtils
	{
	public:
		// Read entire file into string
		static std::string readFile(const char* filepath)
		{
			std::ifstream file(filepath);

			if (!file.is_open())
			{
				LOG_ERROR("Failed to open file: {}", filepath);
				ENGINE_ASSERT(false, "File not found");
				return "";
			}

			std::stringstream buffer;
			buffer << file.rdbuf();
			file.close();

			return buffer.str();
		}

		// Read entire binary file into byte vector
		static std::vector<uint8_t> readBinary(const char* filepath)
		{
			// Open in binary mode, seek to end to get size
			std::ifstream file(filepath, std::ios::binary | std::ios::ate);

			if (!file.is_open())
			{
				LOG_ERROR("Failed to open binary file: {}", filepath);
				ENGINE_ASSERT(false, "Binary file not found");
				return {};
			}

			// Get file size from current position (end of file)
			std::streamsize size = file.tellg();

			// Seek back to beginning
			file.seekg(0, std::ios::beg);

			// Allocate buffer for entire file
			std::vector<uint8_t> buffer(size);

			// Read entire file into buffer
			if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
			{
				LOG_ERROR("Failed to read binary file: {}", filepath);
				ENGINE_ASSERT(false, "Binary file read failed");
				return {};
			}

			file.close();

			LOG_INFO("Read {} bytes from binary file: {}", size, filepath);
			return buffer;
		}
	};
}