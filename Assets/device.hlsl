#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#include <Core.hlsli>

#pragma Parameter srv Texture

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);

struct v2f {
	float4 pos : SV_Position;
	float4 pack0 : TEXCOORD0;
	float4 pack1 : TEXCOORD1;
};

v2f vsmain(float3 vertex : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD0) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	 
	o.pos = mul(Camera.ViewProjection, wp);
	o.pack0.xyz = mul(float4(normal, 1), Object.WorldToObject).xyz;
	o.pack1.xyz = wp.xyz;
	o.pack0.w = uv.x;
	o.pack1.w = uv.y;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	return Texture.Sample(Sampler, float2(i.pack0.w, i.pack1.w));
}