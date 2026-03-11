cmake_minimum_required(VERSION 3.20)
project(HobEngine C CXX)

# ============================================================================
# C++ AND C STANDARDS
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================================
# EXPORT COMPILE COMMANDS (for clangd)
# ============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# BUILD CONFIGURATIONS
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose Debug or Release" FORCE)
endif()

# ============================================================================
# PREPROCESSOR DEFINES
# ============================================================================
if(WIN32)
    add_compile_definitions(_WINDOWS)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(_DEBUG)
    else()
        add_compile_definitions(NDEBUG)
    endif()
endif()

# ============================================================================
# VENDORED LIBRARIES
# ============================================================================

# SDL3 (vendored on both platforms)
add_library(SDL3 SHARED IMPORTED)
if(WIN32)
    set_target_properties(SDL3 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/windows/SDL3.dll"
        IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/windows/libSDL3.dll.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/vendor/SDL3/include"
    )
else()
    set_target_properties(SDL3 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/linux/libSDL3.so"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/vendor/SDL3/include"
    )
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../vendor/SDL3/lib/linux")
    set(CMAKE_BUILD_RPATH "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/linux")
endif()

# GLAD (compile from source)
add_library(glad STATIC
    vendor/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    vendor/glad/include
)

# GLM (header-only)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    vendor/glm
)

# ImGui (compile from source)
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui PUBLIC
    vendor/imgui
    vendor/imgui/backends
    ${CMAKE_SOURCE_DIR}/vendor/SDL3/include
    ${CMAKE_SOURCE_DIR}/vendor/glad/include
)

# stb_image (compile from source)
add_library(stb_image STATIC
    vendor/stb_image/stb_image.cpp
)
target_include_directories(stb_image PUBLIC
    vendor/stb_image
)

# ============================================================================
# OPENGL
# ============================================================================
find_package(OpenGL REQUIRED)

# ============================================================================
# ENGINE STATIC LIBRARY
# ============================================================================
file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS "src/*.h")

add_library(HobEngineLib STATIC
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_include_directories(HobEngineLib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/events
    ${CMAKE_SOURCE_DIR}/src/input
    ${CMAKE_SOURCE_DIR}/src/math
    ${CMAKE_SOURCE_DIR}/src/renderer
    ${CMAKE_SOURCE_DIR}/src/scene
    ${CMAKE_SOURCE_DIR}/vendor/SDL3/include
    ${CMAKE_SOURCE_DIR}/vendor/glad/include
    ${CMAKE_SOURCE_DIR}/vendor/glm
    ${CMAKE_SOURCE_DIR}/vendor/imgui
    ${CMAKE_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_SOURCE_DIR}/vendor/stb_image
)

target_link_libraries(HobEngineLib PUBLIC
    imgui
    SDL3
    glad
    glm
    stb_image
    OpenGL::GL
)

# ============================================================================
# HELPER: DEFINES AN EXAMPLE EXECUTABLE WITH ALL BOILERPLATE
# ============================================================================
function(add_example TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    if(WIN32)
        target_link_options(${TARGET_NAME} PRIVATE -mconsole)
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE HobEngineLib)

    # Copy SDL3 library
    if(WIN32)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/windows/SDL3.dll"
            $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMENT "Copying SDL3.dll for ${TARGET_NAME}"
        )
    else()
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/vendor/SDL3/lib/linux/libSDL3.so"
            $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMENT "Copying libSDL3.so for ${TARGET_NAME}"
        )
    endif()

    # Copy assets
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
        COMMENT "Copying assets for ${TARGET_NAME}"
    )
endfunction()

# ============================================================================
# EXAMPLES
# ============================================================================
add_example(01_comprehensive "examples/01_comprehensive/01_comprehensive.cpp")
add_example(02_modelshowcase "examples/02_modelshowcase/02_modelshowcase.cpp")

# ============================================================================
# COMPILER FLAGS
# ============================================================================
if(MSVC)
    target_compile_options(HobEngineLib PRIVATE /W3)
else()
    target_compile_options(HobEngineLib PRIVATE -Wall -Wextra -pedantic)
endif()
