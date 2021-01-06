#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 0) uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 sampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = sampleSphericalMap(normalize(localPos));
    vec3 color = texture(equirectangularMap, uv).rgb;

    outFragColor = vec4(color, 1.0);
}