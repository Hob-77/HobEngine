#version 460 core

out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoords;

// Material properties
uniform vec3 u_Material_Ambient;
uniform vec3 u_Material_Diffuse;
uniform vec3 u_Material_Specular;
uniform float u_Material_Shininess;
uniform float u_Material_Alpha;

// Texture maps
uniform bool u_Material_HasDiffuseMap;
uniform sampler2D u_Material_DiffuseMap;

uniform bool u_Material_HasSpecularMap;
uniform sampler2D u_Material_SpecularMap;

uniform bool u_Material_HasNormalMap;

uniform bool u_Material_HasEmissiveMap;
uniform sampler2D u_Material_EmissiveMap;

// Camera
uniform vec3 u_CameraPos;

// Multiple Lights System
#define MAX_LIGHTS 16

struct Light {
    vec3 position;
    vec3 color;
};

uniform Light u_Lights[MAX_LIGHTS];
uniform int u_LightCount;

// Calculate lighting contribution from a single light
vec3 calculatePhongLight(Light light, vec3 baseColor, vec3 specularColor, vec3 norm)
{
    // Ambient (simple approximation)
    vec3 ambient = 0.2 * baseColor * light.color;
    
    // Diffuse
    vec3 lightDir = normalize(light.position - v_FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * light.color;
    
    // Specular
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material_Shininess);
    vec3 specular = spec * specularColor * light.color;
    
    return ambient + diffuse + specular;
}

void main()
{
    // Get base color (from texture or material)
    vec3 baseColor = u_Material_HasDiffuseMap 
        ? texture(u_Material_DiffuseMap, v_TexCoords).rgb 
        : u_Material_Diffuse;
    
    // Get specular color
    vec3 specularColor = u_Material_HasSpecularMap 
        ? texture(u_Material_SpecularMap, v_TexCoords).rgb 
        : u_Material_Specular;
    
    // Normal
    vec3 norm = normalize(v_Normal);
    
    // === FIXED: Ambient added ONCE, not per light ===
    // Calculate average light color for ambient
    vec3 avgLightColor = vec3(0.0);
    for(int i = 0; i < u_LightCount; i++)
    {
        avgLightColor += u_Lights[i].color;
    }
    avgLightColor /= float(u_LightCount);
    
    // Ambient (global, added once)
    vec3 ambient = 0.2 * baseColor * avgLightColor;
    
    // Accumulate diffuse + specular from all lights
    vec3 totalDiffuseSpecular = vec3(0.0);
    for(int i = 0; i < u_LightCount; i++)
    {
        // Diffuse
        vec3 lightDir = normalize(u_Lights[i].position - v_FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * baseColor * u_Lights[i].color;
        
        // Specular
        vec3 viewDir = normalize(u_CameraPos - v_FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material_Shininess);
        vec3 specular = spec * specularColor * u_Lights[i].color;
        
        totalDiffuseSpecular += diffuse + specular;
    }
    // === END FIX ===
    
    // Emissive (if present)
    vec3 emissive = u_Material_HasEmissiveMap 
        ? texture(u_Material_EmissiveMap, v_TexCoords).rgb 
        : vec3(0.0);
    
    // Final color: ambient (once) + all lights' contributions
    vec3 result = ambient + totalDiffuseSpecular + emissive;
    
    // Apply alpha for transparency
    FragColor = vec4(result, u_Material_Alpha);
}