#pragma Parameter cbuf			Material
#pragma Parameter float3		Color			 (1,1,1)
#pragma Parameter float			Alpha			 1
#pragma Parameter float3		Emission		 (0,0,0)
#pragma Parameter float			Smoothness		 1
#pragma Parameter float			Metallic		 1
#pragma Parameter float3		LightAmbientSpec (1,1,1)
#pragma Parameter float3		LightAmbientDiff (1,1,1)
#pragma Parameter float4		LightColor		 (0,0,0,0)
#pragma Parameter float4		LightPosition	 (0,0,0,0)
#pragma Parameter float4		LightDirection	 (0,0,0,0)

#define RootSigPBR "CBV(b2, visibility=SHADER_VISIBILITY_PIXEL),"

struct MaterialBuffer {
	float3 baseColor;
	float alpha;
	float3 emission;
	float smoothness;
	float metallic;

	float3 lightAmbientSpec;
	float3 lightAmbientDiff;
	float4 lightColor;
	float4 lightPosition;
	float4 lightDirection;
};
ConstantBuffer<MaterialBuffer> Material : register(b2);

#define PI 3.14159265359
#define INV_PI 0.31830988618

// Unity BRDF from Unity built-in shaders

float Pow5(float x) {
	float x2 = x * x;
	return x2 * x2 * x;
}

float DisneyDiffuse(float NdotV, float NdotL, float LdotH, float perceptualRoughness) {
	float fd90 = 0.5 + 2 * LdotH * LdotH * perceptualRoughness;
	// Two schlick fresnel term
	float lightScatter = (1 + (fd90 - 1) * Pow5(1 - NdotL));
	float viewScatter = (1 + (fd90 - 1) * Pow5(1 - NdotV));

	return lightScatter * viewScatter;
}
float SmithJointGGXVisibilityTerm(float NdotL, float NdotV, float roughness) {
	float a = roughness;
	float lambdaV = NdotL * (NdotV * (1 - a) + a);
	float lambdaL = NdotV * (NdotL * (1 - a) + a);

	return 0.5f / (lambdaV + lambdaL + 1e-5f);
}

float GGXTerm(float NdotH, float roughness) {
	float a2 = roughness * roughness;
	float d = (NdotH * a2 - NdotH) * NdotH + 1.0f;
	return INV_PI * a2 / (d * d + 1e-7f);
}
float3 FresnelTerm(float3 F0, float cosA) {
	float t = Pow5(1 - cosA);   // ala Schlick interpoliation
	return F0 + (1 - F0) * t;
}
float3 FresnelLerp(float3 F0, float3 F90, float cosA) {
	float t = Pow5(1 - cosA);   // ala Schlick interpoliation
	return lerp(F0, F90, t);
}

#define COLORSPACE_GAMMA

#ifdef COLORSPACE_GAMMA
#define unity_ColorSpaceDielectricSpec float4(0.220916301, 0.220916301, 0.220916301, 1.0 - 0.220916301)
#else // Linear values
#define unity_ColorSpaceDielectricSpec float4(0.04, 0.04, 0.04, 1.0 - 0.04) // standard dielectric reflectivity coef at incident angle (= 4%)
#endif

float OneMinusReflectivityFromMetallic(float metallic) {
	// We'll need oneMinusReflectivity, so
	//   1-reflectivity = 1-lerp(dielectricSpec, 1, metallic) = lerp(1-dielectricSpec, 0, metallic)
	// store (1-dielectricSpec) in unity_ColorSpaceDielectricSpec.a, then
	//   1-reflectivity = lerp(alpha, 0, metallic) = alpha + metallic*(0 - alpha) =
	//                  = alpha - metallic * alpha
	float oneMinusDielectricSpec = unity_ColorSpaceDielectricSpec.a;
	return oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
}
float3 DiffuseAndSpecularFromMetallic(float3 albedo, float metallic, out float3 specColor, out float oneMinusReflectivity) {
	specColor = lerp(unity_ColorSpaceDielectricSpec.rgb, albedo, metallic);
	oneMinusReflectivity = OneMinusReflectivityFromMetallic(metallic);
	return albedo * oneMinusReflectivity;
}

float3 UnityBRDF(float3 albedo, float perceptualRoughness, float metallic, float3 normal, float3 viewDir, float3 lightDir, float3 lightColor, float3 giDiffuse, float3 giSpecular) {
	float oneMinusReflectivity;
	float3 specColor;
	float3 diffColor = DiffuseAndSpecularFromMetallic(albedo, metallic, specColor, oneMinusReflectivity);

	float3 halfDir = normalize(float3(lightDir) + viewDir);

	// The amount we shift the normal toward the view vector is defined by the dot product.
	float shiftAmount = dot(normal, viewDir);
	normal = shiftAmount < 0.0f ? normal + viewDir * (-shiftAmount + 1e-5f) : normal;
	// A re-normalization should be applied here but as the shift is small we don't do it to save ALU.
	//normal = normalize(normal);

	float nv = dot(normal, viewDir);

	float nl = saturate(dot(normal, lightDir));
	float nh = saturate(dot(normal, halfDir));

	float lv = saturate(dot(lightDir, viewDir));
	float lh = saturate(dot(lightDir, halfDir));

	// Diffuse term
	float diffuseTerm = DisneyDiffuse(nv, nl, lh, perceptualRoughness) * nl / PI;

	float roughness = perceptualRoughness * perceptualRoughness;

	// GGX with roughtness to 0 would mean no specular at all, using max(roughness, 0.002) here to match HDrenderloop roughtness remapping.
	roughness = max(roughness, 0.002);
	float V = SmithJointGGXVisibilityTerm(nl, nv, roughness);
	float D = GGXTerm(nh, roughness);

	float specularTerm = V * D; // Torrance-Sparrow model, Fresnel is applied later
	
	#ifdef COLORSPACE_GAMMA
	specularTerm = sqrt(max(1e-4h, specularTerm));
	#endif

	// specularTerm * nl can be NaN on Metal in some cases, use max() to make sure it's a sane value
	specularTerm = max(0, specularTerm * nl);

	// surfaceReduction = Int D(NdotH) * NdotH * Id(NdotL>0) dH = 1/(roughness^2+1)
	float surfaceReduction;
	#ifdef COLORSPACE_GAMMA
	surfaceReduction = 1.0 - 0.28*roughness*perceptualRoughness;      // 1-0.28*x^3 as approximation for (1/(x^4+1))^(1/2.2) on the domain [0;1]
	#else
	surfaceReduction = 1.0 / (roughness*roughness + 1.0);           // fade \in [0.5;1]
	#endif

	surfaceReduction = 1.0 - roughness * perceptualRoughness*surfaceReduction;

	// To provide true Lambert lighting, we need to be able to kill specular completely.
	specularTerm *= any(specColor) ? 1.0 : 0.0;
	half grazingTerm = saturate(1.0 - perceptualRoughness + 1.0 - oneMinusReflectivity);
	return diffColor * (giDiffuse + lightColor * diffuseTerm) +
		specularTerm * lightColor * FresnelTerm(specColor, lh) +
		surfaceReduction * giSpecular * FresnelLerp(specColor, grazingTerm, nv);
}

float LightDistanceAttenuation(float x, float r) {
	float atten = 1 / ((x + 1) * (x + 1));
	float b = 1 / ((r + 1) * (r + 1));
	return (atten - b) / (1 - b);
}
float LightAngleAttenuation(float ldp, float r) {
	return saturate(10 * (max(0, ldp) - r));
}

float3 ShadePoint(float3 color, float metallic, float smoothness, float3 normal, float3 worldPos, float3 view) {
	color *= Material.baseColor;
	smoothness *= Material.smoothness;
	metallic *= Material.metallic;

	float roughness = 1.0 - smoothness;

	float3 giSpecular = Material.lightAmbientSpec;
	float3 giDiffuse = Material.lightAmbientDiff * color;

	float3 ldir = Material.lightDirection.xyz;
	float3 lcol = Material.lightColor.rgb;

	if (Material.lightColor.w > 0) { // point or spot light
		ldir = worldPos - Material.lightPosition.xyz;
		float dist = max(.0001, length(ldir));
		ldir /= dist;
		lcol *= LightDistanceAttenuation(dist, Material.lightPosition.w);
		if ((uint)Material.lightColor.w == 1) lcol *= LightAngleAttenuation(dot(ldir, Material.lightDirection.xyz), Material.lightDirection.w); // spot light
	}

	return max(0, UnityBRDF(color, roughness, metallic, normal, -view, -ldir, lcol, giDiffuse, giSpecular) + Material.emission);
}