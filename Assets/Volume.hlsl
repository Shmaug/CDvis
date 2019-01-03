#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=57, b0)," \
"DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"\
	"addressU=TEXTURE_ADDRESS_BORDER, addressV=TEXTURE_ADDRESS_BORDER, addressW=TEXTURE_ADDRESS_BORDER)"

struct DataBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float4x4 ViewProjection;
	float4 CameraPosition;
	float3 LightDirection;
	float LightDensity;
	float Density;
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

float2 RayCube(float3 ro, float3 rd, float3 extents) {
	float3 aabb[2] = { -extents, extents };
	float3 ird = 1.0 / rd;
	int3 sign = int3(rd.x < 0 ? 1 : 0, rd.y < 0 ? 1 : 0, rd.z < 0 ? 1 : 0);

	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	tmin = (aabb[sign.x].x - ro.x) * ird.x;
	tmax = (aabb[1 - sign.x].x - ro.x) * ird.x;
	tymin = (aabb[sign.y].y - ro.y) * ird.y;
	tymax = (aabb[1 - sign.y].y - ro.y) * ird.y;
	tzmin = (aabb[sign.z].z - ro.z) * ird.z;
	tzmax = (aabb[1 - sign.z].z - ro.z) * ird.z;
	tmin = max(max(tmin, tymin), tzmin);
	tmax = min(min(tmax, tymax), tzmax);

	return float2(tmin, tmax);
}

float4 psmain(v2f i) : SV_Target{
	float3 ro = i.ro;
	float3 rd = normalize(i.rd);

	float2 intersect = RayCube(ro, rd, .5);
	intersect.x = max(0, intersect.x);

	ro += .5; // cube has a radius of .5, transform to UVW space

	float4 color = 0;

	float t = intersect.x;
	float s = (intersect.y - intersect.x) / 256.0;
	for (unsigned int i = 0; i < 256; i++) {
		float3 p = ro + rd * t;

		float r = Volume.SampleLevel(Sampler, p, 0).r;
		float d = Data.Density * r;

		float4 c = float4(d, d, d, d * s);

		float lr = Volume.SampleLevel(Sampler, p - Data.LightDirection * s, 0).r;
		c.rgb *= exp(-lr * Data.LightDensity);

		c.rgb *= c.a;
		color += c * (1 - color.a);

		t += s;

		if (color.a > .98 || t > intersect.y) break;
	}

	color.a = saturate(color.a);
	return color;
}