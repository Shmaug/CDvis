#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#include <Core.hlsli>
#include "PBR.hlsli"

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCommon \
RootSigPBR

struct v2f {
	float4 pos : SV_Position;
	float3 normal : NORMAL;
	float3 worldPos : TEXCOORD0;
};

v2f vsmain(float3 vertex : POSITION, float3 normal : NORMAL) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	float3 wn = mul(float4(normal, 1), Object.WorldToObject).xyz;

	o.pos = mul(Camera.ViewProjection, wp);
	o.normal = wn;
	o.worldPos = wp.xyz;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	float3 view = normalize(i.worldPos - Camera.Position.xyz);

	Surface sfc;
	sfc.albedo = 1.0;
	sfc.metallic = 0.0;
	sfc.roughness = .5;
	sfc.normal = normalize(i.normal);
	sfc.worldPos = i.worldPos;

	float3 light = ShadePoint(sfc, i.pos, view);
	return float4(light, 1.0);
}