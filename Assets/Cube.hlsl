#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#include <Core.hlsli>

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore

struct v2f {
	float4 pos : SV_Position;
	float3 color : COLOR;
};

v2f vsmain(float3 vertex : POSITION, float3 normal : NORMAL) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));

	o.pos = mul(Camera.ViewProjection, wp);
	o.color = normal * .5 + .5;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	return float4(i.color, 1.0);
}