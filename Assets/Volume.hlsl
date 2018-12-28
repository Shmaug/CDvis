#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=52, b0)," \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)"

struct DataBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float4x4 ViewProjection;
	float4 CameraPosition;
};
ConstantBuffer<DataBuffer> Data : register(b0);
Texture3D<float2> Volume : register(t0);
sampler Sampler : register(s0);

struct v2f {
	float4 pos : SV_Position;
	float3 ro : TEXCOORD0;
	float3 rd : TEXCOORD1;
};

v2f vsmain(float3 vertex : POSITION) {
	v2f o;

	float4 wp = mul(Data.ObjectToWorld, float4(vertex, 1.0));
	o.pos = mul(Data.ViewProjection, wp);

	o.ro = mul(Data.WorldToObject, float4(Data.CameraPosition.xyz, 1.0)).xyz;
	o.rd = vertex - o.ro;

	return o;
}

float2 RayCube(float3 ro, float3 rd, float3 b) {
	float t1 = (-b[0] - ro[0]) / rd[0];
	float t2 = (b[0] - ro[0]) / rd[0];

	float tmin = min(t1, t2);
	float tmax = max(t1, t2);

	for (int i = 1; i < 3; ++i) {
		t1 = (-b[i] - ro[i]) / rd[i];
		t2 = (b[i] - ro[i]) / rd[i];

		tmin = max(tmin, min(t1, t2));
		tmax = min(tmax, max(t1, t2));
	}

	return float2(max(0, tmin), tmax);
}

float4 psmain(v2f i) : SV_Target{
	float3 ro = i.ro;
	float3 rd = normalize(i.rd);

	float2 intersect = RayCube(ro, rd, 1.0);

	ro += .5; // cube has a radius of .5, transform to UVW space

	float4 color = 0;

	float t = intersect.x;
	float s = (intersect.y - intersect.x) / 64.0;
	for (unsigned int i = 0; i < 64; i++) {
		float3 p = ro + rd * t;

		float d = Volume.Sample(Sampler, p).r;

		d *= s * (1 - color.a);
		color.rgb += d;
		color.a += d;

		t += s;
	}

	return color;
}