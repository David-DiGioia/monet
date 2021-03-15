#version 460
layout (location = 0) in vec3 aPos;

layout (set = 0, binding = 0) uniform LightBuffer{
    mat4 lightSpaceMatrix;
} lightData;

struct ObjectData {
    mat4 model;
};

// all object matrices
layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[]; // SSBOs can only have unsized arrays
} objectBuffer;

void main()
{
    gl_Position = lightData.lightSpaceMatrix * objectBuffer.objects[gl_BaseInstance].model * vec4(aPos, 1.0);
} 