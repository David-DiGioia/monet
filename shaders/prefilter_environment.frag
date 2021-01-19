#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;

layout (push_constant) uniform PushConstants
{
    mat4 rotationMatrix;
    float roughness;
} constants;

const float PI = 3.14159265359;

float distributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a*a;
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Mirror binary digits about the decimal point
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Randomish sequence that has pretty evenly spaced points
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), radicalInverse_VdC(i));
}  

// biased sample vector (importance sampling)
// remember we're assuming w_o = N = R = V
vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    vec3 up = abs(N.z) < 0.999 ? vec3 (0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main()
{
    // Not normal of the cube, but normal that we're calculating irradiance for
    vec3 N = normalize(localPos);
    vec3 R = N;
    vec3 V = N;

    const uint SAMPLE_COUNT = 4096;
    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importanceSampleGGX(Xi, N, constants.roughness);
        // L is V reflected about H
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float D = distributionGGX(NdotH, constants.roughness);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 

            float resolution = 1024.0; // resolution of source cubemap (per face)
            // with a higher resolution, we should sample coarser mipmap levels
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            // as we take more samples, we can sample from a finer mipmap.
            // And places where H is more likely to be sampled (higher pdf) we
            // can use a finer mipmap, otherwise use courser mipmap.
            // The tutorial treats this portion as optional to reduce noise but I think it's
            // actually necessary for importance sampling to get the correct result
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = constants.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 

            prefilteredColor += textureLod(environmentMap, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    outFragColor = vec4(prefilteredColor, 1.0);
    // outFragColor = vec4(vec3(constants.roughness), 1.0);
}