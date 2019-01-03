#include "Dicom.hpp"

#include <Shlwapi.h>
#include <Texture.hpp>

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

// DCMTK libraries (order matters)
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ofstd.lib")
#pragma comment(lib, "oflog.lib")
#pragma comment(lib, "dcmdata.lib")
#pragma comment(lib, "dcmimgle.lib")

using namespace std;
using namespace DirectX;

shared_ptr<Texture> Dicom::LoadImage(jwstring path, XMFLOAT3 &size) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!", path.c_str());
		return nullptr;
	}

	char pathA[MAX_PATH];
	WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, pathA, (int)path.length() + 1, NULL, NULL);
	
	DicomImage* image = new DicomImage(pathA);
	assert(image != NULL);
	assert(image->getStatus() == EIS_Normal);

	// Get information
	DcmFileFormat fileFormat;
	assert(fileFormat.loadFile(pathA).good());
	DcmDataset* dataset = fileFormat.getDataset();
	double spacingX;
	double spacingY;
	double thickness;
	dataset->findAndGetFloat64(DCM_PixelSpacing, spacingX, 0);
	dataset->findAndGetFloat64(DCM_PixelSpacing, spacingY, 1);
	dataset->findAndGetFloat64(DCM_SliceThickness, thickness, 0);

	unsigned int w = image->getWidth();
	unsigned int h = image->getHeight();
	unsigned int d = 1;

	// volume size in meters
	size.x = .001f * (float)spacingX * w;
	size.y = .001f * (float)spacingY * h;
	size.z = .001f * (float)thickness;

	uint16_t* data = new uint16_t[w * h * d];
	ZeroMemory(data, w * h * d * sizeof(uint16_t));
	
	image->setMinMaxWindow();
	uint16_t* pixelData = (uint16_t*)image->getOutputData(16);
	memcpy(data, pixelData, w * h * sizeof(uint16_t));

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"MRI Volume", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16_UNORM, ALPHA_MODE_UNUSED, 0, data, w * h * d * sizeof(uint16_t), false
	));
	tex->Upload();

	delete[] data;
	delete image;
	return tex;
}

void GetFiles(jwstring folder, jvector<jstring> &files) {
	char pathA[MAX_PATH];
	WideCharToMultiByte(CP_UTF8, 0, folder.c_str(), -1, pathA, (int)folder.length() + 1, NULL, NULL);
	jstring path(pathA);
	jstring d = path + "\\*";

	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileA(d.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		assert(false);
		return;
	}

	do {
		if (ffd.cFileName[0] == L'.') continue;

		jstring c = path + "\\" + jstring(ffd.cFileName);
		//if (GetExt(c) != "dcm") continue;

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// file is a directory
		} else
			files.push_back(GetFullPath(c));
	} while (FindNextFileA(hFind, &ffd) != 0);

	FindClose(hFind);
}
shared_ptr<Texture> Dicom::LoadVolume(jwstring path, DirectX::XMFLOAT3 &size) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!", path.c_str());
		return nullptr;
	}

	jvector<jstring> files;
	GetFiles(path, files);

	jvector<DicomImage*> images(files.size());
	for (unsigned int i = 0; i < files.size(); i++) {
		images.push_back(new DicomImage(files[i].c_str()));
		assert(images[i] != NULL);
		assert(images[i]->getStatus() == EIS_Normal);
	}

	// Get information
	DcmFileFormat fileFormat;
	assert(fileFormat.loadFile(files[0].c_str()).good());
	DcmDataset* dataset = fileFormat.getDataset();
	double spacingX;
	double spacingY;
	double thickness;
	dataset->findAndGetFloat64(DCM_PixelSpacing, spacingX, 0);
	dataset->findAndGetFloat64(DCM_PixelSpacing, spacingY, 1);
	dataset->findAndGetFloat64(DCM_SliceThickness, thickness, 0);

	unsigned int w = images[0]->getWidth();
	unsigned int h = images[0]->getHeight();
	unsigned int d = (unsigned int)images.size();

	// volume size in meters
	size.x = .001f * (float)spacingX * w;
	size.y = .001f * (float)spacingY * h;
	size.z = .001f * (float)thickness * images.size();

	wchar_t buf[128];
	swprintf_s(buf, 128, L"%fm x %fm x %fm\n", size.x, size.y, size.z);
	OutputDebugString(buf);

	uint16_t* data = new uint16_t[w * h * d];
	ZeroMemory(data, w * h * d * sizeof(uint16_t));

	for (unsigned int i = 0; i < images.size(); i++) {
		images[i]->setMinMaxWindow();
		uint16_t* pixelData = (uint16_t*)images[i]->getOutputData(16);
		memcpy(data + w*h*i, pixelData, w * h * sizeof(uint16_t));
	}

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"MRI Volume", w, h, d, 
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16_UNORM, ALPHA_MODE_UNUSED, 0, data, w * h * d * sizeof(uint16_t), false
	));
	tex->Upload();

	for (unsigned int i = 0; i < images.size(); i++)
		delete images[i];

	delete[] data;
	return tex;
}