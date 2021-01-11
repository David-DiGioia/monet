#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 2, binding = 0) uniform sampler2D equirectangularMap;

const float PI = 3.14159265359;
const vec2 invAtan = vec2(1.0 / (2.0 * PI), 1.0 / PI);

vec2 sampleSphericalMap(vec3 v)
{
    // uv.x is theta in spherical coordinates. it's simply the angle in x-z plane.
    // sin(phi) = opp / hyp = y / 1
    // since v is a unit vector. note that phi is the angle between x-z plane and v, which
    // is different than the wikipedia article.
    // so we see uv.y = phi = asin(y)
    // vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));

    // atan gives value in [-pi, pi] and asin gives value in [-pi/2, pi/2]
    // so we normalize to get value in [-0.5, 0.5]
    uv *= invAtan;
    // then shift to get in range [0, 1]
    uv += 0.5;

    return uv;
}


void main()
{
    vec2 uv = sampleSphericalMap(normalize(localPos));
    vec3 color = texture(equirectangularMap, uv).rgb;

    outFragColor = vec4(color, 1.0);
}