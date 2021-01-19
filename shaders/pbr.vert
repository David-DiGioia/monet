#version 460 // version 460 required for indexing into transform array with gl_BaseInstance
#define MAX_NUM_TOTAL_LIGHTS 10

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec2 texCoord;
layout (location = 1) out vec3 fragPos;
layout (location = 2) out vec3 camPos;
layout (location = 3) out mat3 outTBN;
layout (location = 6) out vec3 lightPos[MAX_NUM_TOTAL_LIGHTS];

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 viewProjOrigin;
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

layout (push_constant) uniform PushConstants
{
    vec4 roughness_multiplier; // only x component is used
    mat4 render_matrix;
} constants;

void main()
{
    // gl_BaseInstance is the firstInstance parameter in vkCmdDraw
    // which we can use as an arbitrary integer
    mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
    vec4 worldPos4 = modelMatrix * vec4(vPosition, 1.0f);
    gl_Position = cameraData.viewProj * worldPos4;
    texCoord = vTexCoord;

    // transform TBN vectors from model space to world space
    vec3 T = normalize(vec3(modelMatrix * vec4(vTangent, 0.0)));
    vec3 N = normalize(vec3(modelMatrix * vec4(vNormal, 0.0)));
    // re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T); // bitangent vector
    // this matrix transforms from tangent space to world space
    mat3 TBN = mat3(T, B, N);

    // transpose is the inverse since it's orthonormal
    // TBN = transpose(TBN);

    fragPos = worldPos4.xyz;
    camPos = sceneData.camPos.xyz;
    outTBN = TBN;

    for (int i = 0; i < MAX_NUM_TOTAL_LIGHTS; ++i) {
        lightPos[i] = sceneData.lights[i].position.xyz;
    }
}