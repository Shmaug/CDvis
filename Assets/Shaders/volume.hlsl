#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile LIGHT_DIRECTIONAL LIGHT_SPOT LIGHT_POINT
#pragma multi_compile PLANE
#pragma multi_compile ISO

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=23, b0)," \
"DescriptorTable(SRV(t0), SRV(t1), visibility=SHADER_VISIBILITY_PIXEL)," \
"CBV(b1)," \
"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"\
	"addressU=TEXTURE_ADDRESS_BORDER, addressV=TEXTURE_ADDRESS_BORDER, addressW=TEXTURE_ADDRESS_BORDER)"

struct RootData {
	float3 CameraPosition;
	float Projection43;
	float3 LightPosition;
	float Projection33;
	float3 PlanePoint;
	float Density;
	float3 PlaneNormal;
	float LightDensity;
	float3 LightDirection;
	float Exposure;
	float LightAngle;
	float LightAmbient;
	float ISOValue;
};
struct CBData {
	float4x4 ObjectToView;
	float4x4 WorldToObject;
	float4x4 ViewToObject;
	float4x4 Projection;
};

ConstantBuffer<RootData> Root : register(b0);
ConstantBuffer<CBData> CB : register(b1);
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

	float4 vp = mul(CB.ObjectToView, float4(vertex, 1));
	o.pos = mul(CB.Projection, vp);

	o.ro.xyz = mul(CB.WorldToObject, float4(Root.CameraPosition.xyz, 1)).xyz;
	o.rd.xyz = vertex - o.ro.xyz;
	o.sp.xyz = o.pos.xyw;

	float3 ray = vp.xyz;
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
float RayPlane(float3 ro, float3 rd, float3 planep, float3 planen) {
	float d = dot(planen, rd);
	float t = dot(planep - ro, planen);
	return d > 1e-5 ? (t / d) : (t > 0 ? 1e5 : -1e5);
}

float Density(float3 p) {
	#ifdef PLANE
	float d = (dot((p - .5) - Root.PlanePoint, Root.PlaneNormal) < 0) ? 0 : Volume.SampleLevel(Sampler, p, 0).r;
	#else
	float d = Volume.SampleLevel(Sampler, p, 0).r;
	#endif

	#ifdef ISO
	d = (d > Root.ISOValue) * d;
	#endif

	return d;
}

float LinearDepth(float d) {
	return Root.Projection43 / (d - Root.Projection33);
}

// depth texture to object-space ray depth
float DepthTextureToObjectDepth(float3 ro, float3 viewRay, float3 screenPos) {
	float2 uv = screenPos.xy / screenPos.z;
	uv.y = -uv.y;
	uv = uv * .5 + .5;

	float3 vp = viewRay / viewRay.z;
	vp *= LinearDepth(DepthTexture.Sample(Sampler, uv).r);

	return length(mul(CB.ViewToObject, float4(vp, 1)).xyz - ro);
}

float LightDistanceAttenuation(float x, float r) {
	x += 1;
	return 1 / (x * x);
}
float LightAngleAttenuation(float ldp, float r) {
	return saturate(10 * (max(0, ldp) - r));
}

float4 psmain(v2f i) : SV_Target {
	float3 ro = i.ro.xyz;
	float3 rd = normalize(i.rd.xyz);

	float2 intersect = RayCube(ro, rd, .5);
	intersect.x = max(0, intersect.x);

	#ifdef PLANE
	intersect.x = max(intersect.x, RayPlane(ro, rd, Root.PlanePoint, Root.PlaneNormal));
	#endif
	
	// depth buffer intersection
	float z = DepthTextureToObjectDepth(ro, float3(i.ro.w, i.rd.w, i.sp.w), i.sp.xyz);
	intersect.y = min(intersect.y, z);

	clip(intersect.y - intersect.x);

	ro += .5; // cube has a radius of .5, transform to UVW space

	float3 lightDir = normalize(Root.LightDirection);
	float lightRange = length(lightDir);
	lightDir /= lightRange;

	float4 sum = 0;

	float t = intersect.x;
	float dt = .003;
	float ldt = .03;
	unsigned int steps = (intersect.y - intersect.x) / dt;
	for (unsigned int i = 0; i < steps; i++) {
		if (sum.a > .99 || t > intersect.y) break;

		float3 p = ro + rd * t;

		float den = Density(p);
		float4 col = float4((den * Root.Exposure).xxx, den * Root.Density);
		
		#if defined(LIGHT_DIRECTIONAL)
		// sum density from 2 samples towards the light source
		float ld = Density(p - lightDir * ldt) + Density(p - lightDir * ldt * 2);
		ld *= Root.LightDensity * ldt;
		col.rgb *= exp(-ld) + Root.LightAmbient; // extinction = e^(-x)

		#elif defined(LIGHT_SPOT) || defined(LIGHT_POINT)

		float3 ldir = Root.LightPosition - (p-.5);
		float dist = length(ldir);
		ldir /= dist;

		// sum density from 2 samples towards the light source
		float ld = Density(p + ldir * ldt) + Density(p + ldir * ldt * 2);
		ld *= Root.LightDensity * ldt;
		col.rgb *= exp(-ld) // extinction = e^(-x)
			* LightDistanceAttenuation(dist, lightRange * 1000.0f)
		#ifdef LIGHT_SPOT
			* LightAngleAttenuation(dot(lightDir, -ldir), Root.LightAngle)
		#endif
			+ Root.LightAmbient;
		#endif

		col.rgb *= col.a;
		sum += col * (1 - sum.a);

		t += dt;
	}

	sum.a = saturate(sum.a);
	return sum;
}