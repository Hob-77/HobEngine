# HobEngine

> This project is a continuation of [GameEngine](https://github.com/Hob-77/GameEngine).

## Overview

HobEngine is a 3D rendering engine written in C++20, built with minimal dependencies over roughly 500 hours across 2 months. Many projects led up to this point including a 2D engine, each one building toward the goal of tackling 3D graphics seriously through primary sources rather than tutorials.

The study began with Casey Muratori's Handmade Hero, The Cherno's Game Engine Series, Game Engine Architecture by Jason Gregory, and Real-Time Rendering by Tomas Akenine-Möller, Eric Haines, and Naty Hoffman. The intent was to synthesize those into a system built on understood foundations. Every architectural decision in this engine has a concrete reason behind it.

The rendering pipeline is a forward renderer with a fully API-agnostic interface layer. Scene, Camera, Material, and all engine logic have zero OpenGL in them. The OpenGL backend implements IRenderDevice, IShader, ITexture, IMesh, IFramebuffer, and IDebugRenderer. Replacing OpenGL with Vulkan means implementing those interfaces, not rewriting the engine. The render queue sorts by shader then material then distance, producing a 98% reduction in material bind calls. A centralized renderer class caches OpenGL state, reducing redundant state changes by 81.9%. Frustum culling eliminates 50-95% of objects before they reach the GPU. Instanced rendering reduces 1000 draw calls to one at a 150x measured speedup. These were not micro-optimizations added later, they were designed in from the start because graphics is an optimization-heavy domain and algorithmic decisions compound.

Dependencies were kept minimal intentionally. If the project was starting from nothing, cross-platform compatibility was worth thinking about from the start rather than retrofitting it later. All dependencies are vendored and version-locked.

Development started in Visual Studio with MSVC then migrated fully to Neovim with clangd LSP and Clang/LLVM. The migration happened for concrete reasons. MSVC uses a 1.5x vector growth factor and Clang uses 2.0x, which matters when minimizing reallocations in performance-critical systems. Clang also enforces stricter C++20 compliance than MSVC, which caught real bugs and produced a measured 67% FPS improvement in debug mode. The tooling structure that resulted is clean: the compiler handles errors, Clang-Tidy handles static analysis and recommendations, and IntelliSense handles editor features only.

The result is a capable forward renderer with zero memory leaks, zero crashes, and an architecture designed to grow. Physics, animation, audio, and networking are out of scope for this phase.

---

## Windows

Precompiled binaries are available on the [releases page](https://github.com/Hob-77/HobEngine/releases) for those who do not want to build from source.

For those who want to build from source, the instructions below require MSYS2 with the UCRT64 environment.

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

### 6. Run an example
```bash
.\build\01_comprehensive.exe   # Full feature demo
.\build\02_modelshowcase.exe   # OBJ model loading demo
```

---

## Linux (Debian/Ubuntu)

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

### 4. Run an example
```bash
./build/01_comprehensive   # Full feature demo
./build/02_modelshowcase   # OBJ model loading demo
```

---

## Usage

Both examples share the same controls.

### 01_comprehensive

Renders all 10 MeshFactory primitives across two rows with a three-point lighting setup and a skybox. Demonstrates the full rendering pipeline including frustum culling, material batching, three-pass transparency, and instanced rendering.

Row 1: Cube, Sphere, Cylinder, Plane, Quad

Row 2: Cone, Pyramid, Capsule, Torus, Skybox Cube

### 02_modelshowcase

Loads and renders a Nissan Skyline GT-R OBJ model with MTL material support across multiple submeshes. Five lights surround the model evenly. Demonstrates the OBJ loader with multi-material submesh rendering.

Model: [Nissan Skyline GT-R](https://poly.pizza/m/a_HKCtYAv2W) via Poly Pizza

---

### Controls

**Camera**

| Key | Action |
|-----|--------|
| W A S D | Move |
| Mouse | Look |
| Shift | Sprint |
| Space / LCtrl | Fly up / down |
| Tab | Toggle mouse lock |
| R | Reset camera |

**Debug**

| Key | Action |
|-----|--------|
| P | Wireframe mode |
| B | Bounding spheres |
| V | AABBs |
| L | Light positions |
| K | Skybox |
| ESC | Exit |
