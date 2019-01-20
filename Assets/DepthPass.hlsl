#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#include <Core.hlsli>

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS)," \
RootSigCore

struct v2f {
	float4 pos : SV_Position;
	float z : TEXCOORD0;
};

v2f vsmain(float3 vertex : POSITION) {
	v2f o;
	float4 vp = mul(Camera.View, mul(Object.ObjectToWorld, float4(vertex, 1)));
	o.pos = mul(Camera.Projection, vp);
	o.z = vp.z;
	return o;
}

float4 psmain(v2f i) : SV_Target{
	return float4(i.z, i.z, i.z, 1.0);
}