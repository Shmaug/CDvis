#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile LIGHTING
#pragma multi_compile PLANE

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=62, b0)," \
"DescriptorTable(SRV(t0), SRV(t1), visibility=SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"\
	"addressU=TEXTURE_ADDRESS_BORDER, addressV=TEXTURE_ADDRESS_BORDER, addressW=TEXTURE_ADDRESS_BORDER)"

struct DataBuffer {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float4x4 ViewProjection;
	float3 CameraPosition;
	float Density;
	float3 LightDirection;
	float LightDensity;
	float4 Plane;
	float Projection43;
	float Projection33;
};
ConstantBuffer<DataBuffer> Data : register(b0);
Texture3D<float2> Volume : register(t0);
Texture2D<float> DepthTexture : register(t1);
sampler Sampler : register(s0);

struct v2f {
	float4 pos : SV_Position;
	float4 ro : TEXCOORD0;
	float4 rd : TEXCOORD1;
	float4 sp : TEXCOORD2;
};

v2f vsmain(float3 vertex : POSITION) {
	v2f o;

	float4 wp = mul(Data.ObjectToWorld, float4(vertex, 1));
	o.pos = mul(Data.ViewProjection, wp);

	o.ro.xyz = mul(Data.WorldToObject, float4(Data.CameraPosition.xyz, 1)).xyz;
	o.rd.xyz = vertex - o.ro.xyz;
	o.sp.xyz = o.pos.xyw;

	float3 ray = wp.xyz - Data.CameraPosition.xyz;
	o.ro.w = ray.x;
	o.rd.w = ray.y;
	o.sp.w = ray.z;

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
float RayPlane(float3 ro, float3 rd, float4 plane) {
	float d = dot(plane.xyz, rd);
	return d > 1e-3 ? (dot(plane.xyz * plane.w - ro, plane.xyz) / d) : 0;
}

float LinearDepth(float d) {
	return Data.Projection43 / (d - Data.Projection33);
}

float4 psmain(v2f i) : SV_Target {
	float3 ro = i.ro.xyz;
	float3 rd = normalize(i.rd.xyz);
	float3 wrd = normalize(float3(i.ro.w, i.rd.w, i.sp.w));

	float2 intersect = RayCube(ro, rd, .5);
	intersect.x = max(0, intersect.x);

	#ifdef PLANE
	intersect.x = max(0, RayPlane(ro, rd, Data.Plane));
	#endif
	
	// depth buffer intersection
	i.sp /= i.sp.z;
	i.sp.y = -i.sp.y;
	float2 uv = i.sp.xy * .5 + .5;
	float3 p = Data.CameraPosition.xyz + wrd * LinearDepth(DepthTexture.Sample(Sampler, uv).r);
	p = mul(Data.WorldToObject, float4(p, 1)).xyz;
	float z = length(p - ro);

	intersect.y = min(intersect.y, z);

	clip(intersect.y - intersect.x);

	ro += .5; // cube has a radius of .5, transform to UVW space

	float4 sum = 0;

	float t = intersect.x;
	float dt = .003;
	unsigned int steps = (intersect.y - intersect.x) / dt;
	for (unsigned int i = 0; i < steps; i++) {
		if (sum.a > .99 || t > intersect.y) break;

		float3 p = ro + rd * t;

		float raw = Volume.SampleLevel(Sampler, p, 0).r;
		float den = Data.Density * raw;
		den = (den > .2) * den;

		float4 col = float4(lerp(float3(.4, .4, .5), float3(1, 1, 1), saturate(den * 3)), den);

		#ifdef LIGHTING
		float lr = (Volume.SampleLevel(Sampler, p - Data.LightDirection * dt * .5, 0).r * Data.Density - den) * Data.LightDensity;
		col.rgb *= exp(-lr); // extinction = e^(-x)
		//lr += den;
		//col.rgb += lr * exp(-lr + 1); // inscatter = x*e^(-x+1)
		#endif

		col.a *= .4;
		col.rgb *= col.a;
		sum += col * (1 - sum.a);

		t += dt;
	}

	sum.a = saturate(sum.a);
	return sum;
}