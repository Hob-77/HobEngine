#version 460 core

// Per-vertex attributes (from mesh)
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

// Per-instance attributes (from instance buffer)
// mat4 takes 4 consecutive locations (3, 4, 5, 6)
layout(location = 3) in mat4 aInstanceMatrix;

// Uniforms (shared across all instances)
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CameraPos;

// Outputs to fragment shader (FIXED: match basic.frag)
out vec3 v_FragPos;      // Changed from vWorldPos
out vec3 v_Normal;       // Changed from vNormal
out vec2 v_TexCoords;     // Changed from vUV

void main()
{
    // Use instance matrix instead of u_Model
    mat4 modelMatrix = aInstanceMatrix;
    
    vec4 worldPos = modelMatrix * vec4(aPosition, 1.0);
    v_FragPos = worldPos.xyz;
    
    // Transform normal (assumes uniform scale)
    // For non-uniform scale, use: transpose(inverse(mat3(modelMatrix)))
    v_Normal = mat3(modelMatrix) * aNormal;
    
    v_TexCoords = aUV;
    
    gl_Position = u_Projection * u_View * worldPos;
}