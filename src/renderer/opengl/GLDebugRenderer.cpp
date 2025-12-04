#include "renderer/opengl/GLDebugRenderer.h"
#include "renderer/camera/Camera.h"
#include "renderer/camera/FPSCamera.h"
#include "core/Window.h"
#include "core/Logger.h"
#include "scene/Transform.h"
#include <glad/glad.h>

namespace Engine
{
    // Static member definitions
    std::vector<GLDebugRenderer::DebugLine> GLDebugRenderer::s_lines;
    std::unique_ptr<GLShader> GLDebugRenderer::s_shader = nullptr;
    GLuint GLDebugRenderer::s_vao = 0;
    GLuint GLDebugRenderer::s_vbo = 0;
    bool GLDebugRenderer::s_initialized = false;
    bool GLDebugRenderer::s_depthTestEnabled = false;
    float GLDebugRenderer::s_lineWidth = 1.0f;

    void GLDebugRenderer::staticInit()
    {
        if (s_initialized)
        {
            LOG_WARN("GLDebugRenderer already initialized");
            return;
        }

        LOG_INFO("Initializing GLDebugRenderer");

        // Create simple debug shader
        const char* vertexShader = R"(
            #version 460 core
            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aColor;

            uniform mat4 u_ViewProjection;

            out vec3 vColor;

            void main()
            {
                vColor = aColor;
                gl_Position = u_ViewProjection * vec4(aPosition, 1.0);
            }
        )";

        const char* fragmentShader = R"(
            #version 460 core
            in vec3 vColor;
            out vec4 FragColor;

            void main()
            {
                FragColor = vec4(vColor, 1.0);
            }
        )";

        s_shader = std::make_unique<GLShader>(vertexShader, fragmentShader);

        // Create VAO and VBO for dynamic line rendering
        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);

        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

        // Setup vertex attributes (interleaved: pos, color, pos, color)
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Color
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
            (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        s_initialized = true;
        LOG_INFO("GLDebugRenderer initialized successfully");
    }

    void GLDebugRenderer::staticShutdown()
    {
        if (!s_initialized) return;

        LOG_INFO("Shutting down GLDebugRenderer");

        if (s_vbo != 0)
        {
            glDeleteBuffers(1, &s_vbo);
            s_vbo = 0;
        }

        if (s_vao != 0)
        {
            glDeleteVertexArrays(1, &s_vao);
            s_vao = 0;
        }

        s_shader.reset();
        s_lines.clear();
        s_initialized = false;

        LOG_INFO("GLDebugRenderer shutdown complete");
    }

    void GLDebugRenderer::staticDrawLine(const vec3& start, const vec3& end, const vec3& color)
    {
        ensureInitialized();
        s_lines.push_back({ start, color, end, vec3(0) });
    }

    void GLDebugRenderer::staticDrawSphere(const vec3& center, float radius,
        const vec3& color, int segments)
    {
        ensureInitialized();

        const float PI = 3.14159265359f;
        float angleStep = 2.0f * PI / segments;

        // Draw three perpendicular circles (XY, XZ, YZ planes)

        // XY circle
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * angleStep;
            float angle2 = (i + 1) * angleStep;

            vec3 p1(center.x + radius * cosf(angle1),
                center.y + radius * sinf(angle1),
                center.z);
            vec3 p2(center.x + radius * cosf(angle2),
                center.y + radius * sinf(angle2),
                center.z);

            staticDrawLine(p1, p2, color);
        }

        // XZ circle
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * angleStep;
            float angle2 = (i + 1) * angleStep;

            vec3 p1(center.x + radius * cosf(angle1),
                center.y,
                center.z + radius * sinf(angle1));
            vec3 p2(center.x + radius * cosf(angle2),
                center.y,
                center.z + radius * sinf(angle2));

            staticDrawLine(p1, p2, color);
        }

        // YZ circle
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * angleStep;
            float angle2 = (i + 1) * angleStep;

            vec3 p1(center.x,
                center.y + radius * cosf(angle1),
                center.z + radius * sinf(angle1));
            vec3 p2(center.x,
                center.y + radius * cosf(angle2),
                center.z + radius * sinf(angle2));

            staticDrawLine(p1, p2, color);
        }
    }

    void GLDebugRenderer::staticDrawBox(const vec3& center, const vec3& halfSize,
        const vec3& color)
    {
        ensureInitialized();

        // 8 corners of the box
        vec3 corners[8] = {
            center + vec3(-halfSize.x, -halfSize.y, -halfSize.z),
            center + vec3(halfSize.x, -halfSize.y, -halfSize.z),
            center + vec3(halfSize.x,  halfSize.y, -halfSize.z),
            center + vec3(-halfSize.x,  halfSize.y, -halfSize.z),
            center + vec3(-halfSize.x, -halfSize.y,  halfSize.z),
            center + vec3(halfSize.x, -halfSize.y,  halfSize.z),
            center + vec3(halfSize.x,  halfSize.y,  halfSize.z),
            center + vec3(-halfSize.x,  halfSize.y,  halfSize.z)
        };

        // Bottom face
        staticDrawLine(corners[0], corners[1], color);
        staticDrawLine(corners[1], corners[2], color);
        staticDrawLine(corners[2], corners[3], color);
        staticDrawLine(corners[3], corners[0], color);

        // Top face
        staticDrawLine(corners[4], corners[5], color);
        staticDrawLine(corners[5], corners[6], color);
        staticDrawLine(corners[6], corners[7], color);
        staticDrawLine(corners[7], corners[4], color);

        // Vertical edges
        staticDrawLine(corners[0], corners[4], color);
        staticDrawLine(corners[1], corners[5], color);
        staticDrawLine(corners[2], corners[6], color);
        staticDrawLine(corners[3], corners[7], color);
    }

    void GLDebugRenderer::staticDrawOBB(const vec3& center, const vec3& halfSize,
        const mat4& rotation, const vec3& color)
    {
        ensureInitialized();

        // 8 corners of the box in local space
        vec3 localCorners[8] = {
            vec3(-halfSize.x, -halfSize.y, -halfSize.z),
            vec3(halfSize.x, -halfSize.y, -halfSize.z),
            vec3(halfSize.x,  halfSize.y, -halfSize.z),
            vec3(-halfSize.x,  halfSize.y, -halfSize.z),
            vec3(-halfSize.x, -halfSize.y,  halfSize.z),
            vec3(halfSize.x, -halfSize.y,  halfSize.z),
            vec3(halfSize.x,  halfSize.y,  halfSize.z),
            vec3(-halfSize.x,  halfSize.y,  halfSize.z)
        };

        // Transform corners to world space
        vec3 worldCorners[8];
        for (int i = 0; i < 8; ++i)
        {
            vec4 rotated = rotation * vec4(localCorners[i], 1.0f);
            worldCorners[i] = vec3(rotated) + center;
        }

        // Bottom face (indices 0,1,2,3)
        staticDrawLine(worldCorners[0], worldCorners[1], color);
        staticDrawLine(worldCorners[1], worldCorners[2], color);
        staticDrawLine(worldCorners[2], worldCorners[3], color);
        staticDrawLine(worldCorners[3], worldCorners[0], color);

        // Top face (indices 4,5,6,7)
        staticDrawLine(worldCorners[4], worldCorners[5], color);
        staticDrawLine(worldCorners[5], worldCorners[6], color);
        staticDrawLine(worldCorners[6], worldCorners[7], color);
        staticDrawLine(worldCorners[7], worldCorners[4], color);

        // Vertical edges connecting bottom to top
        staticDrawLine(worldCorners[0], worldCorners[4], color);
        staticDrawLine(worldCorners[1], worldCorners[5], color);
        staticDrawLine(worldCorners[2], worldCorners[6], color);
        staticDrawLine(worldCorners[3], worldCorners[7], color);
    }

    void GLDebugRenderer::staticDrawFrustum(const mat4& viewProj, const vec3& color)
    {
        ensureInitialized();

        // Extract frustum corners from inverse view-projection matrix
        mat4 invVP = inverse(viewProj);

        // NDC cube corners (normalized device coordinates: -1 to +1)
        vec4 ndcCorners[8] = {
            // Near plane (z = -1)
            vec4(-1, -1, -1, 1),  // Bottom-left-near
            vec4(1, -1, -1, 1),  // Bottom-right-near
            vec4(1,  1, -1, 1),  // Top-right-near
            vec4(-1,  1, -1, 1),  // Top-left-near
            // Far plane (z = +1)
            vec4(-1, -1,  1, 1),  // Bottom-left-far
            vec4(1, -1,  1, 1),  // Bottom-right-far
            vec4(1,  1,  1, 1),  // Top-right-far
            vec4(-1,  1,  1, 1)   // Top-left-far
        };

        // Transform NDC corners to world space
        vec3 worldCorners[8];
        for (int i = 0; i < 8; ++i)
        {
            vec4 worldPos = invVP * ndcCorners[i];
            worldCorners[i] = vec3(worldPos) / worldPos.w;  // Perspective divide
        }

        // Draw near plane (indices 0,1,2,3)
        staticDrawLine(worldCorners[0], worldCorners[1], color);
        staticDrawLine(worldCorners[1], worldCorners[2], color);
        staticDrawLine(worldCorners[2], worldCorners[3], color);
        staticDrawLine(worldCorners[3], worldCorners[0], color);

        // Draw far plane (indices 4,5,6,7)
        staticDrawLine(worldCorners[4], worldCorners[5], color);
        staticDrawLine(worldCorners[5], worldCorners[6], color);
        staticDrawLine(worldCorners[6], worldCorners[7], color);
        staticDrawLine(worldCorners[7], worldCorners[4], color);

        // Draw connecting lines from near to far
        staticDrawLine(worldCorners[0], worldCorners[4], color);
        staticDrawLine(worldCorners[1], worldCorners[5], color);
        staticDrawLine(worldCorners[2], worldCorners[6], color);
        staticDrawLine(worldCorners[3], worldCorners[7], color);
    }

    void GLDebugRenderer::staticDrawArrow(const vec3& start, const vec3& end,
        const vec3& color, float tipSize)
    {
        ensureInitialized();

        // Draw main line
        staticDrawLine(start, end, color);

        // Calculate arrow tip
        vec3 direction = normalize(end - start);
        vec3 perpendicular1, perpendicular2;

        // Find two perpendicular vectors
        if (abs(direction.x) > 0.9f)
        {
            perpendicular1 = normalize(cross(direction, vec3(0, 1, 0)));
        }
        else
        {
            perpendicular1 = normalize(cross(direction, vec3(1, 0, 0)));
        }
        perpendicular2 = normalize(cross(direction, perpendicular1));

        // Arrow tip base
        vec3 tipBase = end - direction * tipSize;

        // Four lines forming the cone
        vec3 tip1 = tipBase + perpendicular1 * tipSize * 0.3f;
        vec3 tip2 = tipBase - perpendicular1 * tipSize * 0.3f;
        vec3 tip3 = tipBase + perpendicular2 * tipSize * 0.3f;
        vec3 tip4 = tipBase - perpendicular2 * tipSize * 0.3f;

        staticDrawLine(end, tip1, color);
        staticDrawLine(end, tip2, color);
        staticDrawLine(end, tip3, color);
        staticDrawLine(end, tip4, color);
    }

    void GLDebugRenderer::staticDrawAxes(const vec3& position, float size)
    {
        ensureInitialized();

        // X axis - Red
        staticDrawArrow(position, position + vec3(size, 0, 0), vec3(1, 0, 0), size * 0.2f);

        // Y axis - Green
        staticDrawArrow(position, position + vec3(0, size, 0), vec3(0, 1, 0), size * 0.2f);

        // Z axis - Blue
        staticDrawArrow(position, position + vec3(0, 0, size), vec3(0, 0, 1), size * 0.2f);
    }

    void GLDebugRenderer::staticDrawAxesTransform(const Transform& transform, float size)
    {
        ensureInitialized();

        vec3 pos = transform.position;
        mat4 model = transform.getModelMatrix();

        // Extract transformed axes from model matrix
        vec3 right = vec3(model[0]) * size;
        vec3 up = vec3(model[1]) * size;
        vec3 forward = vec3(model[2]) * size;

        // Draw axes in object space
        staticDrawArrow(pos, pos + right, vec3(1, 0, 0), size * 0.2f);    // X - Red
        staticDrawArrow(pos, pos + up, vec3(0, 1, 0), size * 0.2f);       // Y - Green
        staticDrawArrow(pos, pos + forward, vec3(0, 0, 1), size * 0.2f);  // Z - Blue
    }

    void GLDebugRenderer::staticDrawGrid(float size, int divisions, const vec3& color)
    {
        ensureInitialized();

        float halfSize = size * 0.5f;
        float step = size / divisions;

        // Lines parallel to X axis
        for (int i = 0; i <= divisions; ++i)
        {
            float z = -halfSize + i * step;
            staticDrawLine(vec3(-halfSize, 0, z), vec3(halfSize, 0, z), color);
        }

        // Lines parallel to Z axis
        for (int i = 0; i <= divisions; ++i)
        {
            float x = -halfSize + i * step;
            staticDrawLine(vec3(x, 0, -halfSize), vec3(x, 0, halfSize), color);
        }
    }

    void GLDebugRenderer::staticClear()
    {
        s_lines.clear();
    }

    void GLDebugRenderer::staticSetLineWidth(float width)
    {
        s_lineWidth = width;
        glLineWidth(width);
    }

    void GLDebugRenderer::staticSetDepthTest(bool enabled)
    {
        s_depthTestEnabled = enabled;
        LOG_INFO("Debug depth test: {}", enabled ? "enabled" : "disabled");
    }

    void GLDebugRenderer::ensureInitialized()
    {
        if (!s_initialized)
        {
            staticInit();
        }
    }

    void GLDebugRenderer::uploadLinesToGPU()
    {
        if (s_lines.empty()) return;

        // Convert lines to interleaved vertex data
        std::vector<float> vertexData;
        vertexData.reserve(s_lines.size() * 12); // 2 vertices * 6 floats per line

        for (const auto& line : s_lines)
        {
            // Start vertex
            vertexData.push_back(line.start.x);
            vertexData.push_back(line.start.y);
            vertexData.push_back(line.start.z);
            vertexData.push_back(line.color.x);
            vertexData.push_back(line.color.y);
            vertexData.push_back(line.color.z);

            // End vertex
            vertexData.push_back(line.end.x);
            vertexData.push_back(line.end.y);
            vertexData.push_back(line.end.z);
            vertexData.push_back(line.color.x);
            vertexData.push_back(line.color.y);
            vertexData.push_back(line.color.z);
        }

        // Upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            vertexData.size() * sizeof(float),
            vertexData.data(),
            GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLDebugRenderer::staticRender(const CameraBase& camera, const Window& window, IRenderer& renderer)
    {
        ensureInitialized();

        if (!s_lines.empty())  // Only render if there's something to draw
        {
            // Upload lines to GPU
            uploadLinesToGPU();

            // Get view-projection matrix
            mat4 viewProj = camera.getViewProjectionMatrix(window.getAspectRatio());

            // Set OpenGL state
            if (s_depthTestEnabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            glLineWidth(s_lineWidth);

            // Render
            s_shader->bind();
            s_shader->setUniform("u_ViewProjection", viewProj);
            glBindVertexArray(s_vao);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_lines.size() * 2));
            glBindVertexArray(0);
            s_shader->unbind();
        }

        // CRITICAL: Always restore state (even if we didn't render anything)
        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
    }

}