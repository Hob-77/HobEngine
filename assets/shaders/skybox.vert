#version 460 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoords;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main()
{
    // Use position as texture coordinates (direction vector from center)
    v_TexCoords = a_Position;
    
    // CRITICAL: Remove translation from view matrix (keep rotation only)
    // This makes skybox appear infinitely far away
    mat4 skyView = mat4(mat3(u_View));  // mat4(mat3()) strips translation
    
    // Transform to clip space
    vec4 pos = u_Projection * skyView * vec4(a_Position, 1.0);
    
    // TRICK: Set z = w so depth becomes 1.0 (farthest possible)
    // After perspective divide: z/w = w/w = 1.0
    gl_Position = pos.xyww;
}