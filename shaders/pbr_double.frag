#version 460
#define MAX_NUM_TOTAL_LIGHTS 20

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 tangentFragPos;
layout (location = 2) in vec3 tangentCamPos;
layout (location = 3) in vec3 tangentLightPos[MAX_NUM_TOTAL_LIGHTS];

layout (location = 0) out vec4 outFragColor;

struct Light {
    vec4 position;  // w is unused
    vec4 color;     // w is for intensity
};

layout (set = 0, binding = 1) uniform SceneData {
    vec4 ambientColor;
    vec4 sunDirection;
    vec4 sunColor; // w is for sun power
    vec4 camPos; // w is unused
    Light lights[MAX_NUM_TOTAL_LIGHTS];
    int numLights;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D diffuseTex;
layout (set = 2, binding = 1) uniform sampler2D normalTex;
layout (set = 2, binding = 2) uniform sampler2D roughnessTex;
layout (set = 2, binding = 3) uniform sampler2D aoTex;

double metallic = 0.0f;

const double PI = 3.141592653589793238462643383279;

// F0 is the surface reflection at zero incidence (looking directly at surface)
dvec3 fresnelSchlick(double cosTheta, dvec3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - float(cosTheta), 0.0), 5.0);
}

double distributionGGX(dvec3 N, dvec3 H, double roughness)
{
    double a = roughness * roughness;
    double a2 = a*a;
    double NdotH = max(dot(N, H), 0.0);
    double NdotH2 = NdotH * NdotH;

    double num = a2;
    double denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

double geometrySchlickGGX(double NdotV, double roughness)
{
    double r = (roughness + 1.0);
    double k = (r * r) / 8.0;

    double num = NdotV;
    double denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

double geometrySmith(dvec3 N, dvec3 V, dvec3 L, double roughness)
{
    double NdotV = max(dot(N, V), 0.0);
    double NdotL = max(dot(N, L), 0.0);
    double ggx2 = geometrySchlickGGX(NdotV, roughness);
    double ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main()
{
    dvec3 diffuse = texture(diffuseTex, texCoord).rgb;
    // obtain normal from normal map in range [0,1]
    dvec3 normal = texture(normalTex, texCoord).rgb;
    // transform normal dvector to range [-1, 1]
    normal = normalize(normal * 2.0 - 1.0);
    double roughness = texture(roughnessTex, texCoord).r;
    double ao = texture(aoTex, texCoord).r;

    dvec3 N = normal;
    dvec3 V = normalize(tangentCamPos - tangentFragPos);

    dvec3 Lo = dvec3(0.0);
    for (int i = 0; i < sceneData.numLights; ++i) {
        dvec3 L = normalize(tangentLightPos[i] - tangentFragPos);
        dvec3 H = normalize(V + L);

        double dist = distance(tangentFragPos, tangentLightPos[i]);
        double attentuation = 1.0 / (dist * dist);
        dvec3 light = sceneData.lights[i].color.xyz * sceneData.lights[i].color.w;
        dvec3 radiance = light * attentuation;

        // approximate IOR of dialetric materials as 0.04
        dvec3 F0 = dvec3(0.04);
        F0 = mix(F0, diffuse, metallic);
        dvec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        double NDF = distributionGGX(N, H, roughness);
        double G = geometrySmith(N, V, L, roughness);

        dvec3 numerator = NDF * G * F;
        double denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        // Cook-Torrance BRDF
        dvec3 specular = numerator / max(denominator, 0.001);

        // energy of light that gets reflected
        dvec3 kS = F;
        // energy of light that gets refracted
        dvec3 kD = dvec3(1.0) - kS;

        kD *= 1.0 - metallic;

        double NdotL = max(dot(N, L), 0.0);
        Lo += (kD * diffuse / PI + specular) * radiance * NdotL;
    }

    dvec3 ambient = sceneData.ambientColor.xyz * diffuse * ao;
    dvec3 color = ambient + Lo;
    // tonemap using Reinhard operator (this should really be done in post probably)
    color = color / (color + 1.0);

    outFragColor = vec4(color, 1.0);
}
