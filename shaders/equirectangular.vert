#version 460
layout (location = 0) in vec3 vPosition;

layout (location = 0) out vec3 localPos;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
} cameraData;

void main()
{
    localPos = vPosition;
    gl_Position = cameraData.viewProj * vec4(localPos, 1.0);
}
