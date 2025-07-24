#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerMetallicRoughnessMap;
layout (set = 1, binding = 3) uniform sampler2D samplerEmissiveMap;
layout (set = 1, binding = 4) uniform sampler2D samplerDepthMap[6];

layout (set = 0, binding = 1) uniform LightDir {
    vec4 lightPos[6];
} lightDir;

layout (location = 0) in vec3 inNormal;   // normal in world space in vec3 Normal
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;    // view vector in world space camPos-worldPos
layout (location = 4) in vec3 inWorldPos;    // position in world space
layout (location = 5) in vec4 inTangent;
layout (location = 6) in vec4 inLightSpacePos[6];

layout (location = 0) out vec4 outFragColor;

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.5f;
layout (constant_id = 2) const bool USE_METALLIC_TEXTURE = false;
layout (constant_id = 3) const float METALLIC_FACTOR = 1.0f;
layout (constant_id = 4) const float ROUGHNESS_FACTOR = 1.0f;
layout (constant_id = 5) const float AO_FACTOR = 1.0f;
layout (constant_id = 6) const float COLOR_FACTOR_R = 1.0f;
layout (constant_id = 7) const float COLOR_FACTOR_G = 1.0f;
layout (constant_id = 8) const float COLOR_FACTOR_B = 1.0f;
layout (constant_id = 9) const float COLOR_FACTOR_A = 1.0f;
layout (constant_id = 10) const bool USE_NORMAL_MAP = false;
layout (constant_id = 11) const bool USE_OCCLUSION_TEXTURE = false;
layout (constant_id = 12) const bool USE_EMISSIVE_TEXTURE = false;
layout (constant_id = 13) const float EMISSIVE_FACTOR_R = 0.0;
layout (constant_id = 14) const float EMISSIVE_FACTOR_G = 0.0;
layout (constant_id = 15) const float EMISSIVE_FACTOR_B = 0.0;
layout (constant_id = 16) const bool USE_PCF = true;
layout (constant_id = 17) const float LIGHT_STRENGTH = 1.0f;
layout (constant_id = 18) const float AMBIENT_STRENGTH = 0.01f;

const vec3 lightColors[6] = vec3[](
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, 1.0)
);

// uniform vec3 cameraPosition;
const float exposure = 2.2;

// const int DEBUG_LIGHT_INDEX = 5;

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------

// Shadow Mapping
float textureProj(vec4 shadowCoord, vec2 off, int index)
{
	float shadow = 1.0;
    // vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    // projCoords = projCoords * 0.5 + 0.5; // Convert to [0,1] UV
    vec3 projCoords = shadowCoord.xyz;
	if ( projCoords.z < 1.0 && projCoords.z > 0.0 ) 
	{
        float bias = 0.005;
        float dist = texture(samplerDepthMap[index], projCoords.xy + off).r;
        if (projCoords.z - bias > dist)
        {
            shadow = 0.0;
        }
	}
	return shadow;
}

float filterPCF(vec4 sc, int index)
{
	ivec2 texDim = textureSize(samplerDepthMap[index], 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 0;
    if (USE_PCF) {
        range = 1; // 1 for 3x3, 2 for 5x5
    }
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), index);
			count++;
		}
	
	}
	return shadowFactor / count;
}

// Debugging
const float zNear = 0.1;
const float zFar = 1000.0;
float LinearizeDepth(float depth)
{
  float n = zNear;
  float f = zFar;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}
// ----------------------------------------------------------------------------
void main()
{	
    vec3 N = normalize(inNormal);
    vec3 V = normalize(inViewVec);
    vec3 R = reflect(-V, N); 
    vec3 T = normalize(inTangent.xyz);
    vec3 B = normalize(cross(N, T) * inTangent.w); 
    mat3 TBN = mat3(T, B, N);
    if (USE_NORMAL_MAP) {
        vec3 tangentNormal = texture(samplerNormalMap, inUV).rgb * 2.0 - 1.0;
        // tangentNormal = normalize(tangentNormal);
        N = normalize(TBN * tangentNormal);
        // N = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
    } else {
        N = normalize(N);
    }
    //material prop
    vec4 albedo = texture(samplerColorMap, inUV);
    if (albedo.a==0.0) {
        albedo = vec4(1.0);
    }
    albedo *= vec4(COLOR_FACTOR_R, COLOR_FACTOR_G, COLOR_FACTOR_B, COLOR_FACTOR_A);
    float metallic = USE_METALLIC_TEXTURE ? texture(samplerMetallicRoughnessMap, inUV).b * METALLIC_FACTOR : METALLIC_FACTOR;
    metallic = clamp(metallic, 0.0, 1.0);
    float roughness = USE_METALLIC_TEXTURE ? texture(samplerMetallicRoughnessMap, inUV).g * ROUGHNESS_FACTOR : ROUGHNESS_FACTOR;
    roughness = clamp(roughness, 0.045, 1.0);
    float ao = USE_OCCLUSION_TEXTURE ? texture(samplerMetallicRoughnessMap, inUV).r * AO_FACTOR : AO_FACTOR;

    vec3 emissiveFactor = vec3(EMISSIVE_FACTOR_R, EMISSIVE_FACTOR_G, EMISSIVE_FACTOR_B);
    vec3 emissive = emissiveFactor;

    if (USE_EMISSIVE_TEXTURE) {
        emissive *= texture(samplerEmissiveMap, inUV).rgb;
        emissive = pow(emissive, vec3(2.2));
    }
    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use their albedo color as F0 (metallic workflow)    

    // vec3 albedoColor = pow(albedo.rgb, vec3(2.2));
    vec3 albedoColor = albedo.rgb;
    float alpha = albedo.a;
    if (ALPHA_MASK && alpha < ALPHA_MASK_CUTOFF) {
        discard;
    }
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedoColor, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    // vec3 ambient = vec3(0.0);
    // int i = DEBUG_LIGHT_INDEX;
    for(int i = 0; i < 6; ++i) 
    {
        // calculate per-light radiance
        
        vec3 L = normalize(-lightDir.lightPos[i].xyz);
        vec3 H = normalize(V + L);

        float dotNL = clamp(dot(N, L), 0.0, 1.0);

        if (dotNL > 0.0) {
        
            vec3 radiance = lightColors[i] * LIGHT_STRENGTH;

            // Cook-Torrance BRDF
            float NDF = DistributionGGX(N, H, roughness);   
            float G   = GeometrySmith(N, V, L, roughness);      
            vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
            
            vec3 nominator    = NDF * G * F; 
            float denominator = 4.0 * max(dot(V, N), 0.0) * max(dot(L, N), 0.0) + 0.001f; // 0.001 to prevent divide by zero.
            vec3 brdf = nominator / denominator;
            
            // kS is equal to Fresnel
            vec3 kS = F;
            // for energy conservation, the diffuse and specular light can't
            // be above 1.0 (unless the surface emits light); to preserve this
            // relationship the diffuse component (kD) should equal 1.0 - kS.
            vec3 kD = vec3(1.0) - kS;
            // multiply kD by the inverse metalness such that only non-metals have diffuse lighting,
            // or a linear blend if partly metal (pure metals have no diffuse light).
            kD *= 1.0 - metallic;

            // scale light by NdotL
            float NdotL = max(dot(N, L), 0.0);

            float shadow = filterPCF(inLightSpacePos[i], i);
            // float shadow = 1.0;

            // add to outgoing radiance Lo
            Lo += (kD * albedoColor / PI + brdf) * radiance * NdotL * shadow;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
            // ambient += vec3(0.8) * albedoColor * ao * (1-shadow); 
        }
    }

    // ambient lighting (note that the next IBL tutorial will replace
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(AMBIENT_STRENGTH) * albedoColor * ao;
    // ambient /= 6.0; // average ambient light from all lights

    vec3 color = ambient + Lo + emissive;

    outFragColor = vec4(color, alpha);

    //Diagnostic output for debugging
    // outFragColor = vec4(inUV, 0.0, 1.0);
    // outFragColor = vec4(Lo, 1.0);
    // outFragColor = vec4(normalize(N) * 0.5 + 0.5, 1.0);
    // outFragColor = vec4(vec3(roughness), 1.0);
    // outFragColor = vec4(vec3(texture(samplerDepthMap[1],inLightSpacePos[1].xy).r), 1.0);
    //Debugging depth
    // outFragColor = vec4(inLightSpacePos[5].xyz, 1.0);
    // outFragColor = lightDir.lightPos[2];
    // render depths 
    // int index = 2;
    // vec4 shadowCoord = inLightSpacePos[index];
    // vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    // vec3 projCoords = shadowCoord.xyz;
    // float temp = 0.0;
    // if (projCoords.z > 1.0 || projCoords.z < 0.0) {
    //    temp = 1.0;
    // }
    // projCoords = projCoords * 0.5 + 0.5; // Convert to [0,1] UV
    // float dist = texture(samplerDepthMap[index], projCoords.xy).r;
    // outFragColor = vec4(vec3(projCoords.z), 1.0);
    // outFragColor = vec4(vec3(temp), 1.0);
    // outFragColor = vec4(vec3(dist), 1.0);
    // outFragColor = vec4(projCoords, 1.0);
    // outFragColor = vec4(vec3(filterPCF(inLightSpacePos[2], 2)), 1.0);
}