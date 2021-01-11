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
    vec4 pos = constants.rotationMatrix * vec4(vPosition, 1.0);
    pos.z -= 0.01; // account for floating point error at far plane (shift cube forward a little)
    gl_Position = pos;
}
