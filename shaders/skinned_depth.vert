#version 460
layout (location = 0) in vec3 aPos;
layout (location = 4) in vec4 vJointIndices;
layout (location = 5) in vec4 vJointWeights;

layout (set = 0, binding = 0) uniform LightBuffer{
    mat4 lightSpaceMatrix;
} lightData;

struct ObjectData {
    mat4 model;
};

#define MAX_NUM_JOINTS 128

// all object matrices
layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[]; // SSBOs can only have unsized arrays
} objectBuffer;

layout(set = 2, binding = 0) uniform JointMatrices {
	mat4 jointMatrices[MAX_NUM_JOINTS];
    float jointCount;
} skel;

void main()
{
    mat4 skinMat = 
		vJointWeights.x * skel.jointMatrices[int(vJointIndices.x)] +
		vJointWeights.y * skel.jointMatrices[int(vJointIndices.y)] +
		vJointWeights.z * skel.jointMatrices[int(vJointIndices.z)] +
		vJointWeights.w * skel.jointMatrices[int(vJointIndices.w)];

    vec4 pos = lightData.lightSpaceMatrix * objectBuffer.objects[gl_BaseInstance].model * skinMat * vec4(aPos, 1.0);
    pos.z = max(0.0, pos.z); // shadow pancaking
    gl_Position = pos;
} 