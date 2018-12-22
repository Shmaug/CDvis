#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	      "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
          "DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=36, b0)"

struct DataBuffer {
	float4x4 ObjectToWorld;
	float4x4 ViewProjection;
	float4 CameraPosition;
};
ConstantBuffer<DataBuffer> Data : register(b0);

struct v2f {
	float4 pos : SV_Position;
	float3 worldPos : TEXCOORD0;
};

v2f vsmain(float3 vertex : POSITION) {
	v2f o;

	float4 wp = mul(Data.ObjectToWorld, float4(vertex, 1.0));
	o.pos = mul(Data.ViewProjection, wp);
	o.worldPos = wp.xyz;

	return o;
}

float4 psmain(v2f i) : SV_Target{
	float3 ro = Data.CameraPosition.xyz;
	float3 rd = normalize(i.worldPos - Data.CameraPosition.xyz);

	return 1.0f;
}