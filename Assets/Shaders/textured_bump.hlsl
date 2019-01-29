#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>
#include "PBR.hlsli"

#pragma Parameter tbl Textures 2
#pragma Parameter cbuf TexBuffer
#pragma Parameter float4 TextureST     (1,1,0,0)

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
RootSigPBR \
"DescriptorTable(SRV(t0), SRV(t1), visibility=SHADER_VISIBILITY_PIXEL)," \
"CBV(b3)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct TexBuffer {
	float4 TextureST;
};

Texture2D<float4> Texture : register(t0);
Texture2D<float3> Normal : register(t1);
sampler Sampler : register(s0);
ConstantBuffer<TexBuffer> Tex : register(b2);

struct v2f {
	float4 pos : SV_Position;
#ifndef NOLIGHTING
	float4 tbn0 : TEXCOORD0;
	float4 tbn1 : TEXCOORD1;
	float4 tbn2 : TEXCOORD2;
#endif
	float2 uv : TEXCOORD3;
};

v2f vsmain(float3 vertex : POSITION, float2 uv : TEXCOORD0
#ifndef NOLIGHTING
	, float3 normal : NORMAL, float3 tangent : TANGENT
#endif
	) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	o.pos = mul(Camera.ViewProjection, wp);
#ifndef NOLIGHTING
	float3 wn = mul(float4(normal, 1), Object.WorldToObject).xyz;
	float3 wt = mul(float4(tangent, 1), Object.WorldToObject).xyz;
	float3 wb = cross(wn, wt);
	o.tbn0 = float4(wn.xyz, wp.x);
	o.tbn1 = float4(wt.xyz, wp.y);
	o.tbn2 = float4(wb.xyz, wp.z);
#endif
	o.uv = uv * Tex.TextureST.xy + Tex.TextureST.zw;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	#ifdef NOLIGHTING
	return Texture.Sample(Sampler, i.uv) * float4(Material.baseColor, Material.alpha);
	#else
	float3 worldPos = float3(i.tbn0.w, i.tbn1.w, i.tbn2.w);
	float3 normal = normalize(i.tbn0.xyz);
	float3 tangent = normalize(i.tbn1.xyz);
	float3 binormal = normalize(i.tbn2.xyz);
	float3 bump = Normal.Sample(Sampler, i.uv) * 2 - 1;
	normal = bump.x * tangent + bump.y * binormal + bump.z * normal;

	float4 t = Texture.Sample(Sampler, i.uv);
	return float4(ShadePoint(t.rgb, 1, 1, normalize(normal), worldPos, normalize(worldPos - Camera.Position.xyz)), t.a * Material.alpha);
	#endif
}