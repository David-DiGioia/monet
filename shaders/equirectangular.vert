#version 460
#define MAX_NUM_TOTAL_LIGHTS 10

layout (location = 0) in vec3 vPosition;

layout (location = 0) out vec3 localPos;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 viewProjOrigin; // viewProj without camera translation (for skybox)
    mat4 proj;
    mat4 viewProj;
} cameraData;

struct Light {
    vec4 position;  // w is unused
    vec4 color;     // w is for intensity
};

layout (set = 0, binding = 1) uniform SceneData {
    vec4 ambientColor;
    vec4 sunDirection;
    vec4 sunColor; // w is for sun power
    vec4 camPos; // w is unused
    Light lights[MAX_NUM_TOTAL_LIGHTS];
    int numLights;
} sceneData;

struct ObjectData {
    mat4 model;
};

// all object matrices
layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[]; // SSBOs can only have unsized arrays
} objectBuffer;


void main()
{
    localPos = vPosition;
    vec4 pos = cameraData.viewProjOrigin * vec4(localPos, 1.0);
    gl_Position = pos.xyww; // so the depth is always 1.0
}
