# HobEngine

C++ 3D game engine built with minimal dependencies.

## Migration Notice

This repository is a continuation of [GameEngine](https://github.com/Hob-77/GameEngine).

**What changed:**
- **Build System:** Visual Studio → CMake + Ninja
- **Editor:** Visual Studio → Neovim with clangd LSP
- **Compiler:** MSVC → LLVM/Clang (using GNU libstdc++ from MinGW-w64)
- **Development Environment:** Fully free and cross-platform ready

The codebase is the same, but the development tooling has been completely rebuilt for better performance and portability.

---

## Features

- Custom 3D renderer
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

## Requirements

- MSYS2 (UCRT64 environment)
- MinGW-w64 toolchain (UCRT64)
- LLVM/Clang (UCRT64)
- Clang tools extra: clangd, clang-tidy, clang-format
- CMake 3.20+
- Ninja build system
- Git

---

## Windows (MSYS2 UCRT64) Build Instructions

### 1. Install MSYS2
Download and install MSYS2 from: [https://www.msys2.org/](https://www.msys2.org/)

### 2. Use the UCRT64 environment
Open the terminal named `MSYS2 UCRT64`. All commands below must be run inside this shell.

### 3. Install required packages
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-clang
pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra   # clangd, clang-tidy, clang-format
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ninja
```

### 4. Clone the repository
```bash
git clone https://github.com/Hob-77/HobEngine.git
cd HobEngine
```

### 5. Configure and build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 6. Run
```bash
./build/HobEngine.exe
```

---

## Project Structure
```
HobEngine/
├── CMakeLists.txt
├── src/
│   ├── core/
│   ├── renderer/
│   ├── scene/
│   └── ...
├── vendor/
│   ├── SDL3/
│   ├── glad/
│   ├── glm/
│   ├── imgui/
│   └── stb_image/
└── assets/
```

---

## Dependencies (Vendored)

- **SDL3** - Window management and input
- **GLAD** - OpenGL function loader
- **GLM** - OpenGL Mathematics
- **ImGui** - Immediate mode GUI
- **stb_image** - Image loading

No external package managers required.

---

## Development Tools

- **Editor:** Neovim with clangd LSP
- **Compiler:** LLVM/Clang (clang++)
- **Standard Library:** GNU libstdc++ (MinGW-w64)
- **Build System:** CMake + Ninja
- **Debugger:** RAD Debugger
- **Language Standards:** C++20, C17

**Neovim config:** [https://github.com/Hob-77/nvim_config](https://github.com/Hob-77/nvim_config)

---

## Previous Version

Visual Studio version: [https://github.com/Hob-77/GameEngine](https://github.com/Hob-77/GameEngine)