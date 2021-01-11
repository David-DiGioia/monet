#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 2, binding = 0) uniform samplerCube cubemap;

const float PI = 3.14159265359;
const vec2 invAtan = vec2(1.0 / (2.0 * PI), 1.0 / PI);

void main()
{
    vec3 color = texture(cubemap, normalize(localPos)).rgb;

    outFragColor = vec4(color, 1.0);
}