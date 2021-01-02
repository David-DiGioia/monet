#version 460 // version 460 required for indexing into transform array with gl_BaseInstance

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec2 texCoord;
layout (location = 1) out vec3 worldPos;
layout (location = 2) out vec3 normal;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
} cameraData;

struct ObjectData{
    mat4 model;
};

// all object matrices
layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[]; // SSBOs can only have unsized arrays
} objectBuffer;

// push constants block (not in use currently)
layout (push_constant) uniform constants
{
    vec4 data;
    mat4 render_matrix;
} PushConstants;

void main()
{
    // gl_BaseInstance is the firstInstance parameter in vkCmdDraw
    // which we can use as an arbitrary integer
    mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
    vec4 worldPos4 = modelMatrix * vec4(vPosition, 1.0f);
    gl_Position = cameraData.viewProj * worldPos4;
    texCoord = vTexCoord;
    worldPos = worldPos4.xyz;
    normal = vNormal;
}