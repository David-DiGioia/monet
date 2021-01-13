#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main()
{
    // Not the normal of the cube, but the normal that we're calculating the irradiance for
    vec3 normal = normalize(localPos);

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = cross(up, normal);
    up = cross(normal, right);

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
            // and sin(theta) is form conversion of spherical to cartesian coordinates
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            ++nrSamples;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));

    // irradiance = texture(environmentMap, normal).rgb;
    outFragColor = vec4(irradiance, 1.0);
}