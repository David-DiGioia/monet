#version 460
#define MAX_NUM_TOTAL_LIGHTS 100

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 worldPos;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
} cameraData;

layout (set = 0, binding = 1) uniform SceneData {
    vec4 fogColor; // w is for exponent
    vec4 fogDistance; // x for min, y for max, zw unused
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D tex;
float metallic = 0.0f;
float roughness = 0.3f;
float ao = 1.0f;

const float PI = 3.14159265359;
vec3 camPos = -cameraData.view[3].xyz;
vec3 lightPositions[] = {vec3(0.0, 16.0, 0.0), vec3(5.0, 8.0, 0.0),
    vec3(10.0, 10.0, 0.0), vec3(15.0, 10.0, 0.0)};
vec3 lightColors[] = {vec3(10.0, 10.0, 8.0), vec3(1.0, 1.0, 1.0),
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 1.0)};

// F0 is the surface reflection at zero incidence (looking directly at surface)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughenss)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main()
{
    vec3 albedo = texture(tex, texCoord).xyz;
    vec3 N = normalize(normal);
    vec3 V = normalize(camPos - worldPos);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        vec3 L = normalize(lightPositions[i] - worldPos);
        vec3 H = normalize(V + L);

        float dist = distance(worldPos, lightPositions[i]);
        float attentuation = 1.0 / (dist * dist);
        vec3 radiance = lightColors[i] * attentuation;

        // approximate IOR of dialetric materials as 0.04
        vec3 F0 = vec3(0.04);
        F0 = mix(F0, albedo, metallic);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        // Cook-Torrance BRDF
        vec3 specular = numerator / max(denominator, 0.001);

        // energy of light that gets reflected
        vec3 kS = F;
        // energy of light that gets refracted
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.01) * albedo * ao;
    vec3 color = ambient + Lo;
    // tonemap using Reinhard operator (this should really be done in post probably)
    color = color / (color + 1.0);

    outFragColor = vec4(color, 1.0);
}
