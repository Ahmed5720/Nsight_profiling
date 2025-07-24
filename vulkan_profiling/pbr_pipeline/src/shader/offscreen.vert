#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 depthMVP[6];
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 model;
	int index;
} primitive;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

 
void main()
{
	gl_Position =  ubo.depthMVP[primitive.index] * primitive.model * vec4(inPos, 1.0);
}