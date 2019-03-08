#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile TEXTURED

#include <Core.hlsli>

#pragma Parameter srv Texture
#pragma Parameter cbuf PieBuffer
#pragma Parameter float2 TouchPos     (0,0)

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
RootSigCore \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct PieData {
	float2 touchPos;
};

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);
ConstantBuffer<PieData> Pie : register(b2);

struct v2f {
	float4 pos : SV_Position;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f vsmain(float3 vertex : POSITION, float4 color : COLOR0, float2 uv : TEXCOORD0) {
	v2f o;

	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1));
	 
	o.pos = mul(Camera.ViewProjection, wp);
	o.color = color;
	o.uv = uv;
	
	return o;
}

float4 psmain(v2f i) : SV_Target {
	float4 c = i.color;
	#ifdef TEXTURED
	c *= Texture.Sample(Sampler, i.uv);
	#else
	c.rgb += .5 * pow(1 - saturate(length(i.uv - Pie.touchPos) * 20), 2);
	#endif
	clip(c.a - .2);
	return c;
}