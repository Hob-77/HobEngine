#version 460 core

out vec4 FragColor;

in vec3 v_TexCoords;

uniform samplerCube u_Skybox;

void main()
{
    // Sample cubemap using direction vector
    FragColor = texture(u_Skybox, v_TexCoords);
}