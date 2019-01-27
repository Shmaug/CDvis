#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile NOLIGHTING

#include <Core.hlsli>

#pragma Parameter tbl Textures 2
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
"DescriptorTable(SRV(t0), SRV(t1), visibility=SHADER_VISIBILITY_PIXEL)," \
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
Texture2D<float3> Normal : register(t1);
sampler Sampler : register(s0);
ConstantBuffer<MaterialBuffer> Material : register(b2);

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
	o.uv = uv * Material.TextureST.xy + Material.TextureST.zw;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	float4 color = Texture.Sample(Sampler, i.uv) * Material.Tint;
	#ifndef NOLIGHTING
	float3 worldPos = float3(i.tbn0.w, i.tbn1.w, i.tbn2.w);
	float3 normal = normalize(i.tbn0.xyz);
	float3 tangent = normalize(i.tbn1.xyz);
	float3 binormal = normalize(i.tbn2.xyz);
	float3 bump = Normal.Sample(Sampler, i.uv) * 2 - 1;
	normal = bump.x * tangent + bump.y * binormal + bump.z * normal;

	float3 lightdir;
	float atten;
	if (Material.LightPosition.w) {
		// point light
		lightdir = Material.LightPosition.xyz - worldPos;
		float d = length(lightdir);
		lightdir /= d;
		atten = 1 / (1 + d*d);
	} else {
		// directional light
		atten = 1;
		lightdir = Material.LightPosition.xyz;
	}
	atten *= saturate(dot(normal, lightdir));
	color.rgb *= Material.LightColor.rgb * atten + Material.Ambient.rgb;
	#endif
	return color;
}