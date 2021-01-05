#version 460 // version 460 required for indexing into transform array with gl_BaseInstance
#define MAX_NUM_TOTAL_LIGHTS 20

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec2 texCoord;
layout (location = 1) out vec3 tangentFragPos;
layout (location = 2) out vec3 tangentCamPos;
layout (location = 3) out vec3 tangentLightPos[MAX_NUM_TOTAL_LIGHTS];

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
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
    dmat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
    dvec4 worldPos4 = modelMatrix * dvec4(vPosition, 1.0f);
    gl_Position = vec4(cameraData.viewProj * worldPos4);
    texCoord = vTexCoord;

    // transform TBN dvectors from model space to world space
    dvec3 T = normalize(dvec3(modelMatrix * dvec4(vTangent, 0.0)));
    dvec3 N = normalize(dvec3(modelMatrix * dvec4(vNormal, 0.0)));
    // re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    dvec3 B = cross(N, T); // bitangent dvector
    // this matrix transforms from tangent space to world space
    dmat3 TBN = mat3(T, B, N);
    // transpose is the inverse since it's orthonormal
    TBN = transpose(TBN);

    tangentFragPos = vec3(TBN * worldPos4.xyz);
    tangentCamPos = vec3(TBN * sceneData.camPos.xyz);

    for (int i = 0; i < MAX_NUM_TOTAL_LIGHTS; ++i) {
        tangentLightPos[i] = vec3(TBN * sceneData.lights[i].position.xyz);
    }
}