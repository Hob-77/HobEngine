# HobEngine

C++ 3D game engine built from scratch with OpenGL.

## Migration Notice

This repository is a continuation of [GameEngine](https://github.com/Hob-77/GameEngine).

**What changed:**
- **Build System:** Visual Studio → CMake + Ninja
- **Editor:** Visual Studio → Neovim with clangd LSP
- **Compiler:** MSVC → MinGW-w64 GCC 15.2.0 (UCRT64)
- **Development Environment:** Fully free and cross-platform ready

The codebase is the same, but the development tooling has been completely rebuilt for better performance and portability.

---

## Features

- Custom 3D renderer (OpenGL 4.6)
- Forward rendering with multiple light sources
- Scene management
- Input system
- Post-processing effects
- Skybox rendering with cubemap textures
- Model loading (.obj)

**Performance:**
- Debug: ~2700 FPS
- Release: ~4700 FPS

*Test scene: 11 primitives, 3 lights (forward rendering), skybox with cubemap textures*

---

## Build Instructions

### Requirements
- CMake 3.20+
- Ninja build system
- MinGW-w64 GCC (UCRT64 from MSYS2)
- Git

### Windows (MSYS2)

1. Install MSYS2 from https://www.msys2.org/

2. Install dependencies:
\\\ash
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ninja
\\\

3. Clone repository:
\\\ash
git clone https://github.com/Hob-77/HobEngine.git
cd HobEngine
\\\

4. Configure and build:
\\\ash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
\\\

5. Run:
\\\ash
./build/HobEngine.exe
\\\

---

## Project Structure

\\\
HobEngine/
├── CMakeLists.txt          # Build configuration
├── src/                    # Engine source code
│   ├── core/
│   ├── renderer/
│   ├── scene/
│   └── ...
├── vendor/                 # Vendored dependencies
│   ├── SDL3/
│   ├── glad/
│   ├── glm/
│   ├── imgui/
│   └── stb_image/
└── assets/                 # Shaders, textures
\\\

---

## Dependencies (Vendored)

All dependencies are included:
- **SDL3** - Window management and input
- **GLAD** - OpenGL function loader
- **GLM** - OpenGL Mathematics
- **ImGui** - Immediate mode GUI
- **stb_image** - Image loading

No external package managers required.

---

## Development Tools

- **Editor:** Neovim with clangd LSP
- **Compiler:** GCC 15.2.0 (MinGW-w64 UCRT64)
- **Build System:** CMake + Ninja
- **Debugger:** RAD Debugger
- **Standards:** C++20, C17

**Neovim config:** https://github.com/Hob-77/nvim_config

---

## Previous Version

Visual Studio version: https://github.com/Hob-77/GameEngine
