#version 460

layout (location = 0) in vec3 vPosition;

layout (location = 0) out vec3 localPos;

layout (push_constant) uniform PushConstants
{
    mat4 rotationMatrix; // ensure current cube face is facing camera
} constants;

void main()
{
    localPos = vPosition;
    gl_Position = constants.rotationMatrix * vec4(vPosition, 1.0);
}
