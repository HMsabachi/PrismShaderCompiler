#version 450

#pragma kernel CSEquirectToCube
#pragma kernel CSIrradiance
#pragma kernel CSMipFilter


layout(std430, binding = 0) buffer ParticleData {
    vec4 positions[];
    vec4 velocities[];
};


layout(binding = 1) uniform sampler2D u_InputTex;
layout(binding = 2) uniform samplerCube u_EnvCube;


layout(rgba16f, binding = 3) uniform image2D o_Result2D;
layout(rgba32f, binding = 4) uniform imageCube o_ResultCube;
layout(r32ui, binding = 5) uniform image2D o_Count2D;


layout(location = 0) uniform float u_Roughness;
layout(location = 1) uniform vec4 u_Color;

const float PI = 3.141592;

vec3 GetCubeMapDirection() {
    vec2 uv = gl_GlobalInvocationID.xy / imageSize(o_ResultCube).xy;
    // ...
}


[numthreads(32, 32, 1)]
void CSEquirectToCube() {
    vec3 dir = GetCubeMapDirection();
    vec4 color = texture(u_InputTex, uv);
    imageStore(o_ResultCube, ivec3(gl_GlobalInvocationID), color);
}

[numthreads(8, 8, 64)]
void CSIrradiance() {
    // ...
}

[numthreads(64, 1, 1)]
void CSMipFilter() {
    // ...
}