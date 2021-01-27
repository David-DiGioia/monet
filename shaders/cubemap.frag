#version 460

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 2, binding = 0) uniform samplerCube cubemap;

void main()
{
    vec3 color = texture(cubemap, normalize(localPos)).rgb;

    outFragColor = vec4(color, 1.0);
}