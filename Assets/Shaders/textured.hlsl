#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>
#include "PBR.hlsli"

#pragma Parameter srv Texture
#pragma Parameter cbuf TexBuffer
#pragma Parameter float4 TextureST     (1,1,0,0)

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
RootSigPBR \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"CBV(b3)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct TexBuffer {
	float4 TextureST;
};

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);
ConstantBuffer<TexBuffer> Tex : register(b3);

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
	o.pack0.w = uv.x * Tex.TextureST.x + Tex.TextureST.z;
	o.pack1.w = uv.y * Tex.TextureST.y + Tex.TextureST.w;
#else
	o.uv = uv * Tex.TextureST.xy + Tex.TextureST.zw;
#endif

	return o;
}

float4 psmain(v2f i) : SV_Target{
	#ifndef NOLIGHTING
	float4 t = Texture.Sample(Sampler, float2(i.pack0.w, i.pack1.w));
	return float4(ShadePoint(t.rgb, 1, 1, normalize(i.pack0.xyz), i.pack1.xyz, normalize(i.pack1.xyz - Camera.Position.xyz)), t.a * Material.alpha);
	#else
	return Texture.Sample(Sampler, i.uv) * float4(Material.baseColor, Material.alpha);
	#endif
}