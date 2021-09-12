#version 460
#define MAX_NUM_TOTAL_LIGHTS 10

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 fragPos;
layout (location = 2) in vec4 fragPosLightSpace;
layout (location = 3) in vec3 camPos;
layout (location = 4) in mat3 TBN;
layout (location = 7) in vec3 lightPos[MAX_NUM_TOTAL_LIGHTS];

layout (location = 0) out vec4 outFragColor;

layout (push_constant) uniform PushConstants
{
    vec4 roughness_multiplier; // only x component is used
    mat4 render_matrix;
} constants;

struct Light {
    vec4 position;  // w is unused
    vec4 color;     // w is for intensity
};

layout (set = 0, binding = 1) uniform SceneData {
    mat4 lightSpaceMatrix; // for shadow mapping
    vec4 camPos; // w is unused
    Light lights[MAX_NUM_TOTAL_LIGHTS];
    int numLights;
} sceneData;

layout (set = 0, binding = 2) uniform sampler2D shadowMap;

layout (set = 2, binding = 0) uniform sampler2D diffuseTex;
layout (set = 2, binding = 1) uniform sampler2D normalTex;
layout (set = 2, binding = 2) uniform sampler2D roughnessTex;
layout (set = 2, binding = 3) uniform sampler2D aoTex;
layout (set = 2, binding = 4) uniform sampler2D metalTex;

// diffuse portion of integral
layout (set = 2, binding = 5) uniform samplerCube irradianceMap;
// first portion of specular portion of integral
layout (set = 2, binding = 6) uniform samplerCube prefilterMap;
// second portion of specular portion of integral
layout (set = 2, binding = 7) uniform sampler2D brdfLUT;

const float PI = 3.14159265359;
// Lower values result in darker shadows
const float IRRADIANCE_SHADOW_CLAMP = 0.4;
const float SPECULAR_SHADOW_CLAMP = 0.6;

// Lightest value achieved by fragment in shadow (fragment brightness is unbounded, so this choice must be made)
const float IRRADIANCE_MAX_SHADOW_VALUE = 1.5; // Note: lightest irradiance value in renderdoc ~2.6
const float SPECULAR_MAX_SHADOW_VALUE = 1.3;   // Note: lightest prefilter value in renderdoc ~1.7

// F0 is the surface reflection at zero incidence (looking directly at surface)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
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

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 analytic_lights(vec3 N, vec3 V, vec3 F0, vec3 diffuse, float roughness, float metallic)
{
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < sceneData.numLights; ++i) {
        vec3 L = normalize(lightPos[i] - fragPos);
        vec3 H = normalize(V + L);

        float dist = distance(fragPos, lightPos[i]);
        float attentuation = 1.0 / (dist * dist);
        vec3 light = sceneData.lights[i].color.xyz * sceneData.lights[i].color.w;
        vec3 radiance = light * attentuation;

        F0 = mix(F0, diffuse, metallic);
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
        Lo += (kD * diffuse / PI + specular) * radiance * NdotL;
    }
    return Lo;
}

float shadowCalculation(vec4 fragPosLightSpace) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // fragment's lightspace position is in range [-1, 1] so we map it to rang [0, 1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    // get closest depth from light's persepctive
    // float closestDepth = texture(shadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = clamp(projCoords.z, 0.0, 1.0);

    // check whether current frag pos is in shadow
    // float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec3 diffuse = texture(diffuseTex, texCoord).rgb;

    // obtain normal from normal map in range [0,1]
    vec3 normal = texture(normalTex, texCoord).rgb;
    // transform normal vector to range [-1, 1]
    normal = normalize(normal * 2.0 - 1.0);
    float roughness = texture(roughnessTex, texCoord).g * constants.roughness_multiplier.x;
    // make roughness never exactly 0.0 or 1.0 because discontinuities appear at these values
    roughness = (roughness * 0.999) + 0.0001;

    float ao = texture(aoTex, texCoord).r;
    float metallic = texture(metalTex, texCoord).b;

    // transform normal from tangent space to world space
    vec3 N = TBN * normal;
    vec3 V = normalize(camPos - fragPos);

    // approximate IOR of dialetric materials as 0.04
    vec3 F0 = vec3(0.04);

    vec3 R = reflect(-V, N);

    vec3 Lo = analytic_lights(N, V, F0, diffuse, roughness, metallic);
    
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness); 
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    float shadow = shadowCalculation(fragPosLightSpace);
    vec3 irradiance = texture(irradianceMap, N).rgb;

    // irradiance and prefilteredColor are unbounded, so when not in shadow we don't want to clamp it. But when in shadow
    // we clamp it in the between (0, A) where A is in [IRRADIANCE_SHADOW_CLAMP, 1.0] depending on value of shadow
    irradiance = shadow == 0.0 ? irradiance : clamp(irradiance, 0.0, mix(IRRADIANCE_SHADOW_CLAMP, IRRADIANCE_MAX_SHADOW_VALUE, (1.0 - shadow)));

    diffuse = irradiance * diffuse;

    const float MAX_REFLECTION_LOD = 8.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;

    prefilteredColor = shadow == 0.0 ? prefilteredColor : clamp(prefilteredColor, 0.0, mix(SPECULAR_SHADOW_CLAMP, SPECULAR_MAX_SHADOW_VALUE, (1.0 - shadow)));

    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    // Mirroring roughness below because for some reason the brdfLUT is mirrored??
    vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N, V), 0.0), 1.0 - roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    // we don't multiply specular by kS since we already have a Fresnel multiplication in there
    vec3 ambient = (kD * diffuse + specular) * ao;

    vec3 color = ambient + Lo;
    // tonemap using Reinhard operator (this should really be done in post probably)
    // color = color / (color + 1.0);

    outFragColor = vec4(color, 1.0);
    // outFragColor = vec4(vec3(shadow), 1.0);
}
