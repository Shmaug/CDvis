#pragma warning(disable : 3568) // unrecognized pragma
#pragma rootsig RootSig
#pragma vertex vsmain
#pragma pixel psmain
#pragma multi_compile LIGHT_DIRECTIONAL LIGHT_SPOT LIGHT_POINT
#pragma multi_compile PLANE
#pragma multi_compile ISO
#pragma multi_compile COLOR MASK MASK_DISP INVERT

#define RootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32bitconstants=27, b0)," \
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
	float3 WorldScale;
	float LightIntensity;
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
#ifdef COLOR
Texture3D<float3> Volume : register(t0);
#else
Texture3D<float2> Volume : register(t0);
#endif
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

#ifdef COLOR
#define Epsilon 1e-10

float3 RGBtoHCV(in float3 RGB) {
	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
	float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
	return float3(H, C, Q.x);
}
float3 RGBtoHSV(in float3 RGB) {
	float3 HCV = RGBtoHCV(RGB);
	float S = HCV.y / (HCV.z + Epsilon);
	return float3(HCV.x, S, HCV.z);
}
#endif

float SampleDensity(float3 p) {
	float a;

	#ifdef COLOR
	float3 rgb = Volume.SampleLevel(Sampler, p, 0).rgb;

	float3 hsv = RGBtoHSV(rgb);
	a = (1 - hsv.x) * (hsv.z > .1);
	a *= a;

	#else

	float2 x = Volume.SampleLevel(Sampler, p, 0).rg;

	#ifdef MASK
	a = x.r * x.g;
	#else
	a = x.r;
	#endif

	#endif


	#ifdef PLANE
	a = (dot((p - .5) - Root.PlanePoint, Root.PlaneNormal) < 0) ? 0 : a;
	#endif

	#ifdef INVERT
	a = 1 - a;
	#endif

	#ifdef ISO
	//a = (s.a > Root.ISOValue) * s.a; // cutoff for hard edges
	a = max(0, a - Root.ISOValue); // subtractive for soft edges
	#endif
	
	#if defined(MASK_DISP) && !defined(COLOR)
	a = .01 * a + x.g;
	#endif

	return a * Root.Density;
}
float4 Sample(float3 p) {
	float4 s;

	#ifdef COLOR
	s.rgb = Volume.SampleLevel(Sampler, p, 0).rgb;

	float3 hsv = RGBtoHSV(s.rgb);
	s.a = (1 - hsv.x) * (hsv.z > .1);
	s.a *= s.a;

	s.rgb *= Root.Exposure;

	#else

	float2 x = Volume.SampleLevel(Sampler, p, 0).rg;

	#ifdef MASK
	s = x.r * x.g;
	#else
	s = x.r;
	s.rgb *= Root.Exposure;
	#endif

	#endif


	#ifdef PLANE
	s.a = (dot((p - .5) - Root.PlanePoint, Root.PlaneNormal) < 0) ? 0 : s.a;
	#endif

	#ifdef INVERT
	s.a = 1 - s.a;
	#endif

	#ifdef ISO
	//s.a = (s.a > Root.ISOValue) * s.a; // cutoff for hard edges
	s.a = max(0, s.a - Root.ISOValue); // subtractive for soft edges
	#endif
	
	#if defined(MASK_DISP) && !defined(COLOR)
	s.rgb += float3(.549, 1, .6) * x.g;
	s.a = .01 * s.a + x.g;
	#endif

	return s * Root.Density;
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

	float4 sum = 0;

	#define dt .002
	#define ldt .01
	uint steps = 0;
	float pd = 0;
	for (float t = intersect.x; t < intersect.y;) {
		if (sum.a > .98 || steps > 128) break;

		float3 p = ro + rd * t;
		float4 col = Sample(p);

		if (col.a > .01){
			if (pd < .01) {
				// first time entering volume, binary subdivide to get closer to entrance point
				float t0 = t - dt * 3;
				float t1 = t;
				float tm;
				#define BINARY_SUBDIV tm = (t0 + t1) * .5; p = ro + rd * tm; if (SampleDensity(p)) t1 = tm; else t0 = tm;
				BINARY_SUBDIV
				BINARY_SUBDIV
				BINARY_SUBDIV
				#undef BINARY_SUBDIV
				t = tm;
				col = Sample(p);
			}

			#if defined(LIGHT_DIRECTIONAL)

			float3 uvwldir = -Root.LightDirection / Root.WorldScale;
			float ld = SampleDensity(p + uvwldir * ldt) + SampleDensity(p + uvwldir * ldt * 2);

			ld *= Root.LightDensity * ldt;
			col.rgb *= (exp(-ld) + Root.LightAmbient) * Root.LightIntensity; // extinction = e^(-x)

			#elif defined(LIGHT_SPOT) || defined(LIGHT_POINT)

			float3 wp = (p - .5) * Root.WorldScale;
			float3 ldir = Root.LightPosition - wp;
			float dist = length(ldir);
			ldir /= dist;

			float3 uvwldir = ldir / Root.WorldScale;

			// sum density from 2 samples towards the light source
			float ld = SampleDensity(p + uvwldir * ldt) + SampleDensity(p + uvwldir * ldt * 2);

			dist += 1;

			ld *= Root.LightDensity * ldt;
			col.rgb *= (exp(-ld) // extinction = e^(-x)
				* 1 / (dist * dist) // LightDistanceAttenuation
			#ifdef LIGHT_SPOT
				* saturate(10 * (max(0, dot(Root.LightDirection, -ldir)) - Root.LightAngle)) // LightAngleAttenuation
			#endif
				+ Root.LightAmbient) * Root.LightIntensity;
			#endif

			col.rgb *= col.a;
			sum += col * (1 - sum.a);

			steps++; // only count steps through the volume
		}

		pd = col.a;
		t += col.a > .01 ? dt : dt * 5; // step farther if not in dense part
	}

	sum.a = saturate(sum.a);
	return sum;
}