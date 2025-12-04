#version 460 core

out vec4 FragColor;

in vec2 v_TexCoords;

// Scene texture from framebuffer
uniform sampler2D u_ScreenTexture;

// Effect toggles
uniform bool u_GrayscaleEnabled = true;

void main()
{
    // Sample the scene color from framebuffer
    vec3 color = texture(u_ScreenTexture, v_TexCoords).rgb;
    
    // Apply grayscale effect (if enabled)
    if (u_GrayscaleEnabled)
    {
        // ITU-R BT.601 luminance formula (matches human perception)
        // Green contributes most (58.7%), red medium (29.9%), blue least (11.4%)
        float gray = dot(color, vec3(0.299, 0.587, 0.114));
        color = vec3(gray);
    }
    
    // Output final color
    FragColor = vec4(color, 1.0);
}