#include "VRDial.hpp"

#include <AssetDatabase.hpp>
#include <Mesh.hpp>
#include <Material.hpp>
#include <Texture.hpp>
#include <Shader.hpp>
#include <Font.hpp>

#include <Scene.hpp>
#include <TextRenderer.hpp>

#include "VRDevice.hpp"

using namespace std;
using namespace DirectX;

VRDial::VRDial(const jwstring& name, float val, float min, float max, unsigned int steps)
	: MeshRenderer(name), mValue(val), mMin(min), mMax(max), mSteps(steps) {}
VRDial::~VRDial() {}

void VRDial::Create() {
	auto dialMesh = AssetDatabase::GetAsset<Mesh>(L"dial");
	dialMesh->UploadStatic();
	auto dialTex = AssetDatabase::GetAsset<Texture>(L"dial_diffuse");
	dialTex->Upload();

	auto textured = AssetDatabase::GetAsset<Shader>(L"textured");
	textured->Upload();
	auto dialMaterial = shared_ptr<Material>(new Material(L"Dial", textured));
	dialMaterial->SetTexture("Texture", dialTex, -1);

	SetMesh(dialMesh);
	SetMaterial(dialMaterial);

	mLabel = GetScene()->AddObject<TextRenderer>(L"DialText");
	mLabel->Parent(Parent());
	mLabel->LocalScale(.1f, .1f, .1f);
	mLabel->LocalPosition(LocalPosition().x - .055f, LocalPosition().y, LocalPosition().z);
	mLabel->LocalRotation(XMQuaternionRotationRollPitchYaw(XM_PIDIV2, XM_PI, 0));
	mLabel->Text(mName + L": 0.00");
	mLabel->Font(AssetDatabase::GetAsset<Font>(L"arial"));

	auto textMat = shared_ptr<Material>(new Material(L"Text", textured));
	textMat->EnableKeyword("NOLIGHTING");
	textMat->CullMode(D3D12_CULL_MODE_NONE);
	textMat->Blend(BLEND_STATE_ALPHA);
	mLabel->Material(textMat);

	Value(mValue);
}

float toEuler(const XMFLOAT3& a, float angle) {
	float s = sinf(angle);
	float c = cosf(angle);
	float t = 1.f - c;
	if ((a.x * a.y * t + a.z * s) > 0.998f) {
		return 2.f * atan2f(a.x * sinf(angle * .5f), cosf(angle * .5f));
		//attitude = XM_PIDIV2;
		//bank = 0;
	}
	if ((a.x * a.y * t + a.z * s) < -0.998f) {
		return -2.f * atan2(a.x * sinf(angle * 5.f), cosf(angle * .5f));
		//attitude = -XM_PIDIV2;
		//bank = 0;
	}
	return atan2f(a.y * s - a.x * a.z * t, 1.f - (a.y * a.y + a.z * a.z) * t);
	//attitude = asinf(x * y * t + z * s);
	//bank = atan2f(x * s - y * z * t, 1 - (x * x + z * z) * t);
}

void VRDial::Value(float v) {
	mValue = v;
	float t = (float)(mValue - mMin) / (mMax - mMin);
	float angle = t * (XM_2PI * .6f) + XM_2PI * .2f;
	LocalRotation(XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), angle));
}

void VRDial::Drag(const shared_ptr<Object>& this_obj, const shared_ptr<VRDevice>& device, const XMVECTOR& newPos, const XMVECTOR& newRot) {
	XMVECTOR axis;
	float angle;
	XMQuaternionToAxisAngle(&axis, &angle, newRot);

	XMFLOAT3 axisf;
	XMStoreFloat3(&axisf, axis);

	angle = toEuler(axisf, angle);
	if (angle < 0) angle += XM_2PI;

	angle = fminf(XM_2PI * .8f, fmaxf(XM_2PI * .2f, angle));

	LocalRotation(XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), angle));

	float t = (angle - XM_2PI * .2f) / (XM_2PI * .6f);

	int lstep = (int)(mSteps * (float)(mValue - mMin) / (mMax - mMin) + .5f);
	int step = (int)(t * mSteps + .5f);

	if (step != lstep) {
		int steps = abs(step - lstep);
		device->TriggerHapticPulse(500 * steps);

		mValue = mMin + (mMax - mMin) * t;

		wchar_t txt[32];
		swprintf_s(txt, 32, L"%s: %.2f", mName.c_str(), mValue);
		mLabel->Text(txt);
	}
};