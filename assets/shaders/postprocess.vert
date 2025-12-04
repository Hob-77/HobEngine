#version 460 core

// Input: Screen quad vertices
layout(location = 0) in vec2 a_Position;   // NDC coordinates [-1, 1]
layout(location = 1) in vec2 a_TexCoords;  // UV coordinates [0, 1]

// Output to fragment shader
out vec2 v_TexCoords;

void main()
{
    // Pass through texture coordinates
    v_TexCoords = a_TexCoords;
    
    // Position in clip space (no transformation needed - already in NDC)
    gl_Position = vec4(a_Position, 0.0, 1.0);
}