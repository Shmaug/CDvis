#pragma warning(disable : 3568) // unrecognized pragma

#pragma rootsig RootSig
#pragma compute main
#pragma Parameter tbl Textures

#pragma multi_compile FILL PAINT GROW SHRINK

#define RootSig "DescriptorTable(UAV(u0)), RootConstants(num32bitconstants=24, b0)"

struct DataBuffer {
	float4x4 indexToWorld;
	float3 paintPos0;
	float paintRadius;
	float3 paintPos1;
	float paintValue;
};

RWTexture3D<float2> volume : register(u0);
ConstantBuffer<DataBuffer> Data : register(b0);

const static int3 offsets[6] = {
	int3(-1, 0, 0),
	int3( 1, 0, 0),
	int3(0, -1, 0),
	int3(0,  1, 0),
	int3(0, 0, -1),
	int3(0, 0,  1)
};

float minimum_distance(float3 v, float3 w, float3 p) {
	float3 wv = w - v;
	float l2 = dot(wv, wv);
	float3 pv = p - v;

	if (l2 < l2 < .00001) pv = v + saturate(dot(pv, wv) / l2) * wv - p;

	return dot(pv, pv);
}

[numthreads(8, 8, 8)]
void main(uint3 index : SV_DispatchThreadID) {
	float2 rg = volume[index.xyz];
	float m = rg.g;

#if defined(FILL)

	m = Data.paintValue;

#elif defined(PAINT)

	float3 wp = mul(Data.indexToWorld, float4((float3)index.xyz, 1)).xyz;
	float x = minimum_distance(Data.paintPos0, Data.paintPos1, wp);

	x = 1 - x / Data.paintRadius;
	m = lerp(m, Data.paintValue, saturate(3 * x));

#elif defined(GROW)
	for (uint i = 0; i < 6; i++)
		m = max(m, volume[index + offsets[i]].g);
#elif defined(SHRINK)
	for (uint i = 0; i < 6; i++)
		m = min(m, volume[index + offsets[i]].g);
#endif

	rg.g = m;
	volume[index.xyz] = rg;
}