#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

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
    // Not the normal of the cube, but the normal that we're calculating the irradiance for
    vec3 normal = normalize(localPos);
    vec3 N = normal;

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = cross(up, normal);
    up = cross(normal, right);

    // ------------------------------------------------------------------------------

    const uint SAMPLE_COUNT = 16384;
    float totalWeight = 0.0;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importanceSampleGGX(Xi, N, 1.0);

        // NdotH is equal to cos(theta)
        float NdotH = max(dot(N, H), 0.0);
        // With roughness == 1 in the distribution function we get 1/pi
        float D = 1.0 / PI;
        float pdf = (D * NdotH / (4.0)) + 0.0001; 

        float resolution = 1024.0; // resolution of source cubemap (per face)
        // with a higher resolution, we should sample coarser mipmap levels
        float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
        // as we take more samples, we can sample from a finer mipmap.
        // And places where H is more likely to be sampled (higher pdf) we
        // can use a finer mipmap, otherwise use courser mipmap.
        // The tutorial treats this portion as optional to reduce noise but I think it's
        // actually necessary for importance sampling to get the correct result
        float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

        float mipLevel = 0.5 * log2(saSample / saTexel); 

        irradiance += textureLod(environmentMap, H, mipLevel).rgb * NdotH;
        // irradiance += texture(environmentMap, H).rgb * NdotH;
        totalWeight += NdotH;
    }
    irradiance = (PI * irradiance) / totalWeight;
    // irradiance = (PI * irradiance) / SAMPLE_COUNT;


    /*
    float phiDelta = 0.025;
    float thetaDelta = 0.025;
    int nrSamples = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += phiDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += thetaDelta) {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            // cos(theta) is from dot product part of diffuse term of rendering equation
            // and sin(theta) is from conversion of spherical to cartesian coordinates
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            ++nrSamples;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    */

    // irradiance = texture(environmentMap, normal).rgb;
    outFragColor = vec4(irradiance, 1.0);
}