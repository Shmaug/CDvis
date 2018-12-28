#include "Dicom.hpp"

#include <Shlwapi.h>
#include <Texture.hpp>

using namespace std;
using namespace DirectX;

shared_ptr<Texture> Dicom::LoadImage(jwstring path) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!", path.c_str());
		return nullptr;
	}

	unsigned int w = 32;
	unsigned int h = 32;
	unsigned int d = 32;
	uint16_t* data = new uint16_t[w * h * d];
	ZeroMemory(data, w * h * d * sizeof(uint16_t));

	// TODO

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"MRI Volume", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R8G8_UNORM, ALPHA_MODE_UNUSED, 0, data, w * h * d * sizeof(uint16_t), false
	));
	tex->Upload();
	return tex;
}
shared_ptr<Texture> Dicom::LoadVolume(jwstring path) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!", path.c_str());
		return nullptr;
	}

	unsigned int w = 32;
	unsigned int h = 32;
	unsigned int d = 32;
	uint16_t* data = new uint16_t[w * h * d];
	ZeroMemory(data, w * h * d * sizeof(uint16_t));

	for (unsigned int x = 0; x < w; x++) {
		for (unsigned int y = 0; y < h; y++) {
			for (unsigned int z = 0; z < d; z++) {
				data[z * (w * h) + x + y * w] = max(0, 10 - sqrtf((x - w / 2) * (x - w / 2) + (y - h / 2) * (y - h / 2) + (z - d / 2) * (z - d / 2)));
			}
		}
	}

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"MRI Volume", w, h, d, 
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R8G8_UNORM, ALPHA_MODE_UNUSED, 0, data, w * h * d * sizeof(uint16_t), false
	));
	tex->Upload();
	return tex;
}