#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>

#pragma Parameter srv Texture
#pragma Parameter cbuf Material
#pragma Parameter float4 TextureST     (1,1,0,0)
#pragma Parameter float4 Tint          (1,1,1,1)
#pragma Parameter float4 Ambient       (1,1,1,1)
#pragma Parameter float4 LightPosition (0,0,0,0)
#pragma Parameter float3 LightColor    (0,0,0)

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"CBV(b2)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct MaterialBuffer {
	float4 TextureST;
	float4 Tint;
	float4 Ambient;
	float4 LightPosition; // w is 0 if light is a directional light
	float3 LightColor;
};

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);
ConstantBuffer<MaterialBuffer> Material : register(b2);

struct v2f {
	float4 pos : SV_Position;
#ifndef NOLIGHTING
	float4 pack0 : TEXCOORD0; // normal (xyz) u (w)
	float4 pack1 : TEXCOORD1; // world pos (xyz) v (w)
#else
	float2 uv : TEXCOORD0;
#endif
};

v2f vsmain(float3 vertex : POSITION, float2 uv : TEXCOORD0
#ifndef NOLIGHTING
	, float3 normal : NORMAL
#endif
) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	 
	o.pos = mul(Camera.ViewProjection, wp);
#ifndef NOLIGHTING
	o.pack0.xyz = mul(float4(normal, 1), Object.WorldToObject).xyz;
	o.pack1.xyz = wp.xyz;
	o.pack0.w = uv.x * Material.TextureST.x + Material.TextureST.z;
	o.pack1.w = uv.y * Material.TextureST.y + Material.TextureST.w;
#else
	o.uv = uv * Material.TextureST.xy + Material.TextureST.zw;
#endif

	return o;
}

float4 psmain(v2f i) : SV_Target{
	#ifndef NOLIGHTING
	float4 color = Texture.Sample(Sampler, float2(i.pack0.w, i.pack1.w)) * Material.Tint;
	float3 lightdir;
	float atten;
	if (Material.LightPosition.w) {
		// point light
		lightdir = Material.LightPosition.xyz - i.pack1.xyz;
		float d = length(lightdir);
		lightdir /= d;
		atten = 1 / (1 + d*d);
	} else {
		// directional light
		atten = 1;
		lightdir = Material.LightPosition.xyz;
	}
	atten *= saturate(dot(normalize(i.pack0.xyz), lightdir));
	color.rgb *= Material.LightColor.rgb * atten + Material.Ambient.rgb;
	return color;
	#else
	return Texture.Sample(Sampler, i.uv) * Material.Tint;
	#endif
}