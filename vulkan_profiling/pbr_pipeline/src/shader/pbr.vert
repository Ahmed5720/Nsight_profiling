#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (set = 0, binding = 0) uniform UBOScene 
{
	mat4 projection;
	mat4 view;
	mat4 lightSpaceMatrix[6];
	vec4 viewPos;
} uboScene;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

// const mat4 biasMat = mat4( 
// 	0.5, 0.0, 0.0, 0.0,
// 	0.0, 0.5, 0.0, 0.0,
// 	0.0, 0.0, 1.0, 0.0,
// 	0.5, 0.5, 0.0, 1.0 );

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outWorldPos;
layout (location = 5) out vec4 outTangent;
// layout (location = 6) out vec4 outLightSpacePos[6];

void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	outUV = inUV;
	outTangent = inTangent;
	gl_Position = uboScene.projection * uboScene.view * primitive.model * vec4(inPos.xyz, 1.0);
	
	outNormal = mat3(primitive.model) * inNormal;
	vec4 pos = primitive.model * vec4(inPos, 1.0);
	outWorldPos = pos.xyz;
	outViewVec = uboScene.viewPos.xyz - pos.xyz;
	// for (int i = 0; i < 6; i++) {
	// 	outLightSpacePos[i] = biasMat * uboScene.lightSpaceMatrix[i] * primitive.model * vec4(inPos, 1.0);
	// }
}