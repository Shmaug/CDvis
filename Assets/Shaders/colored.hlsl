#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>
#include "PBR.hlsli"

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
RootSigPBR

struct v2f {
	float4 pos : SV_Position;
	#ifndef NOLIGHTING
	float3 worldNormal : TEXCOORD0; // normal (xyz) u (w)
	float3 worldPos : TEXCOORD1; // world pos (xyz) v (w)
	#endif
};

v2f vsmain(
	float3 vertex : POSITION
#ifndef NOLIGHTING
	, float3 normal : NORMAL
#endif
) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	 
	o.pos = mul(Camera.ViewProjection, wp);
	#ifndef NOLIGHTING
	o.worldNormal = mul(float4(normal, 1), Object.WorldToObject).xyz;
	o.worldPos = wp.xyz;
	#endif
	return o;
}

float4 psmain(v2f i) : SV_Target{
	#ifndef NOLIGHTING
	return float4(ShadePoint(1, 1, 1, normalize(i.worldNormal), i.worldPos, normalize(i.worldPos - Camera.Position.xyz)), Material.alpha);
	#else
	return float4(Material.baseColor, Material.alpha);
	#endif
}