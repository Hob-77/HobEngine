# HobEngine

C++ 3D game engine built with minimal dependencies.

## Migration Notice

This repository is a continuation of [GameEngine](https://github.com/Hob-77/GameEngine).

**What changed:**
- **Build System:** Visual Studio → CMake + Ninja
- **Editor:** Visual Studio → Neovim with clangd LSP
- **Compiler:** MSVC → LLVM/Clang
- **Development Environment:** Fully free and cross-platform (Windows + Linux)

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
- **Cross-platform:** Runs identically on Windows and Linux

*Test scene: 11 primitives, 3 lights (forward rendering), skybox with cubemap textures*

---

## Requirements

### Common (Both Platforms)
- CMake 3.20+
- Ninja build system
- Git
- LLVM/Clang compiler
- clangd (for LSP support)

### Windows-Specific
- MSYS2 (UCRT64 environment)
- MinGW-w64 toolchain (UCRT64)
- Clang tools extra (clang-tidy, clang-format)

### Linux-Specific
- Build essentials (for system libraries)
- Clang/LLVM toolchain
- Clang tools (clang-tidy, clang-format)

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

## Linux (Debian/Ubuntu) Build Instructions

### 1. Install build dependencies
```bash
sudo apt install build-essential cmake ninja-build clang clang++ clangd clang-tidy clang-format git
```

### 2. Clone the repository
```bash
git clone https://github.com/Hob-77/HobEngine.git
cd HobEngine
```

### 3. Configure and build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 4. Run
```bash
./build/HobEngine
```

---

## Project Structure
```
HobEngine/
├── CMakeLists.txt          # Cross-platform build configuration
├── src/
│   ├── core/               # Core engine systems (Window, Logger, Time)
│   ├── renderer/           # OpenGL rendering pipeline
│   │   ├── camera/         # Camera systems (FPS, Orbit)
│   │   ├── opengl/         # OpenGL-specific implementations
│   │   └── interface/      # Renderer abstractions
│   ├── scene/              # Scene management (Objects, Lights, Materials)
│   ├── input/              # Input handling
│   ├── math/               # Math utilities and frustum culling
│   ├── events/             # Event system
│   └── ui/                 # ImGui integration
├── vendor/                 # Vendored dependencies (version-locked)
│   ├── SDL3/
│   │   ├── include/        # SDL3 headers
│   │   └── lib/
│   │       ├── windows/    # SDL3 3.2.2 for Windows (DLL + import lib)
│   │       └── linux/      # SDL3 3.2.2 for Linux (.so)
│   ├── glad/               # OpenGL 4.6 loader
│   ├── glm/                # Math library
│   ├── imgui/              # Immediate mode GUI
│   └── stb_image/          # Image loading
└── assets/
    ├── shaders/            # GLSL vertex/fragment shaders
    ├── textures/           # Textures and cubemaps
    └── models/             # .obj model files
```

---

## Dependencies (Vendored)

All dependencies are **vendored** in the repository to ensure version consistency across contributors and platforms. Every user builds with the exact same library versions regardless of OS.

- **SDL3 3.2.2** - Window management and input
- **GLAD** - OpenGL 4.6 function loader
- **GLM** - OpenGL Mathematics
- **ImGui** - Immediate mode GUI
- **stb_image** - Image loading

**No external package managers required.** Clone and build - the only difference between platforms is the OS.

---

## Development Tools

- **Editor:** Neovim with clangd LSP
- **Compiler:** LLVM/Clang (clang++)
  - Windows: GNU libstdc++ from MinGW-w64
  - Linux: System libstdc++
- **Build System:** CMake + Ninja
- **Debugger:** RAD Debugger (Windows), GDB (Linux)
- **Language Standards:** C++20, C17

**Neovim config:** [https://github.com/Hob-77/nvim_config](https://github.com/Hob-77/nvim_config)

The development environment is identical on both platforms - same compiler (Clang), same CMakeLists.txt, same Neovim config, same build commands.

---

## Previous Version

Visual Studio version: [https://github.com/Hob-77/GameEngine](https://github.com/Hob-77/GameEngine)
