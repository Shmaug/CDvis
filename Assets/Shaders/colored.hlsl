#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>

#pragma Parameter cbuf Material
#pragma Parameter float4 Color         (1,1,1,1)
#pragma Parameter float4 Ambient       (1,1,1,1)
#pragma Parameter float4 LightPosition (0,0,0,0)
#pragma Parameter float3 LightColor    (0,0,0)

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
"CBV(b2)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct MaterialBuffer {
	float4 Color;
	float4 Ambient;
	float4 LightPosition; // w is 0 if light is a directional light
	float3 LightColor;
};

ConstantBuffer<MaterialBuffer> Material : register(b2);

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
	float4 color = Material.Color;
	#ifndef NOLIGHTING
	float3 lightdir;
	float atten;
	if (Material.LightPosition.w) {
		// point light
		lightdir = Material.LightPosition.xyz - i.worldPos;
		float d = length(lightdir);
		lightdir /= d;
		atten = 1 / (1 + d*d);
	} else {
		// directional light
		atten = 1;
		lightdir = Material.LightPosition.xyz;
	}
	atten *= saturate(dot(normalize(i.worldNormal), lightdir));
	color.rgb *= Material.LightColor.rgb * atten + Material.Ambient.rgb;
	#endif
	return color;
}