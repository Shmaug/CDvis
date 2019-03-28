#include "ImageLoader.hpp"

#include <Shlwapi.h>
#include <Texture.hpp>

#include <Graphics.hpp>
#include <CommandQueue.hpp>
#include <CommandList.hpp>
#include <Shader.hpp>
#include <AssetDatabase.hpp>

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

#include <algorithm>

#include "lodepng.hpp"

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

void LoadRawImageSlice(const jstring& path, uint8_t* dst, unsigned int w, unsigned int h) {
	uint8_t* filedata = new uint8_t[w * h * 3];

	ifstream file;
	file.open(path.c_str(), std::ios::binary);
	file.seekg(0, std::ios::end);
	size_t len = file.tellg();
	file.seekg(0, std::ios::beg);
	file.read((char*)filedata, len);
	file.close();


	uint8_t* r = filedata;
	uint8_t* g = filedata + w * h;
	uint8_t* b = filedata + 2 * w * h;

	unsigned int i = 0;

	for (unsigned int x = 0; x < w; x++)
		for (unsigned int y = 0; y < h; y++) {
			if (y < 20 || x < 180 || x > 1830 || y > 1000) continue;
			i = x + y * w;
			dst[4 * i]     = r[i];
			dst[4 * i + 1] = g[i];
			dst[4 * i + 2] = b[i];
		}

	delete[] filedata;
}

shared_ptr<Texture> LoadRawImage(const jstring& path, XMFLOAT3 &size) {
	size = { .67584f, .40128f, .001f };

	unsigned int w = 2048;
	unsigned int h = 1216;
	unsigned int d = 1;

	size.x = .001f * .33f * w;
	size.y = .001f * .33f * h;
	size.z = .001f * d;
	
	uint8_t* data = new uint8_t[w * h * d * 4];
	ZeroMemory(data, w * h * d * 4);

	LoadRawImageSlice(path, data, w, h);

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"RAW Volume", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, data, w * h * d * 4, false
	));
	tex->Upload(D3D12_RESOURCE_FLAG_NONE, false);

	delete[] data;
	return tex;
}
shared_ptr<Texture> LoadRawVolume(const jvector<jstring>& files, XMFLOAT3 &size) {
	unsigned int w = 2048;
	unsigned int h = 1216;
	unsigned int d = (unsigned int)files.size() / 2;

	size.x = .001f * .33f * w;
	size.y = .001f * .33f * h;
	size.z = .001f * (float)files.size();

	uint8_t* data = new uint8_t[w * h * d * 4];
	ZeroMemory(data, w * h * d * 4);

	for (unsigned int i = 0; i < d; i++) {
		LoadRawImageSlice(files[i * 2], data + (4 * w*h*i), w, h);
		if (i % 10 == 0) OutputDebugf(L"%d%%\n", (int)(100.f * (float)i / (float)d + .5f));
	}

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"RAW Volume", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 1, data, w * h * d * 4, false
	));
	tex->Upload(D3D12_RESOURCE_FLAG_NONE, false);

	delete[] data;
	return tex;
}

shared_ptr<Texture> LoadDicomImage(const jstring& path, XMFLOAT3 &size) {
	DicomImage* image = new DicomImage(path.c_str());
	assert(image != NULL);
	assert(image->getStatus() == EIS_Normal);

	// Get information
	DcmFileFormat fileFormat;
	assert(fileFormat.loadFile(path.c_str()).good());
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

	uint16_t* data = new uint16_t[w * h * d * 2];
	ZeroMemory(data, w * h * d * sizeof(uint16_t) * 2);

	image->setMinMaxWindow();
	uint16_t* pixelData = (uint16_t*)image->getOutputData(16);
	//memcpy(data + w * h*i, pixelData, w * h * sizeof(uint16_t));
	unsigned int j = 0;
	for (unsigned int x = 0; x < w; x++)
		for (unsigned int y = 0; y < h; y++) {
			j = 2 * (x + y * w);
			data[j] = pixelData[x + y * w];
			data[j + 1] = 0xFFFF;
		}

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"temp", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16G16_UNORM, 1, data, w * h * d * sizeof(uint16_t) * 2, false
	));
	tex->Upload(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, true);

	delete[] data;
	delete image;
	return tex;
}
shared_ptr<Texture> LoadDicomVolume(const jvector<jstring>& files, XMFLOAT3 &size) {
	struct Slice {
		DicomImage* image;
		double location;
	};

	jvector<Slice> images(files.size());

	// Get information
	double spacingX;
	double spacingY;
	double thickness;

	for (unsigned int i = 0; i < files.size(); i++) {
		DcmFileFormat fileFormat;
		assert(fileFormat.loadFile(files[i].c_str()).good());
		DcmDataset* dataset = fileFormat.getDataset();

		if (i == 0) {
			dataset->findAndGetFloat64(DCM_PixelSpacing, spacingX, 0);
			dataset->findAndGetFloat64(DCM_PixelSpacing, spacingY, 1);
			dataset->findAndGetFloat64(DCM_SliceThickness, thickness, 0);
		}

		double x;
		dataset->findAndGetFloat64(DCM_SliceLocation, x, 0);

		DicomImage* img = new DicomImage(files[i].c_str());
		assert(img != NULL);
		assert(img->getStatus() == EIS_Normal);
		images.push_back({ img, x });
	}

	std::sort(images.data(), images.data() + images.size(), [](const Slice& a, const Slice& b) {
		return a.location < b.location;
	});

	unsigned int w = images[0].image->getWidth();
	unsigned int h = images[0].image->getHeight();
	unsigned int d = (unsigned int)images.size();

	// volume size in meters
	size.x = .001f * (float)spacingX * w;
	size.y = .001f * (float)spacingY * h;
	size.z = .001f * (float)thickness * images.size();

	wchar_t buf[128];
	swprintf_s(buf, 128, L"%fm x %fm x %fm\n", size.x, size.y, size.z);
	OutputDebugString(buf);

	uint16_t* data = new uint16_t[w * h * d * 2];
	ZeroMemory(data, w * h * d * sizeof(uint16_t) * 2);

	for (unsigned int i = 0; i < images.size(); i++) {
		images[i].image->setMinMaxWindow();
		uint16_t* pixelData = (uint16_t*)images[i].image->getOutputData(16);
		//memcpy(data + w * h*i, pixelData, w * h * sizeof(uint16_t));
		unsigned int j = 0;
		for (unsigned int x = 0; x < w; x++)
			for (unsigned int y = 0; y < h; y++) {
				j = 2 * (x + y * w + i * w * h);
				data[j] = pixelData[x + y * w];
				data[j + 1] = 0xFFFF;
			}
	}

	shared_ptr<Texture> tex = shared_ptr<Texture>(new Texture(
		L"MRI Volume", w, h, d,
		D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16G16_UNORM, 1, data, w * h * d * sizeof(uint16_t) * 2, false
	));
	tex->Upload(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, true);

	for (unsigned int i = 0; i < images.size(); i++)
		delete images[i].image;

	delete[] data;
	return tex;
}

shared_ptr<Texture> ImageLoader::LoadImage(const jwstring& path, XMFLOAT3 &size) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!\n", path.c_str());
		return nullptr;
	}

	char pathA[MAX_PATH];
	WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, pathA, (int)path.length() + 1, NULL, NULL);

	jstring ext = GetExtA(pathA);
	if (ext == "dcm")
		return LoadDicomImage(pathA, size);
	else if (ext == "raw")
		return LoadRawImage(pathA, size);
	else
		return nullptr;
}

void GetFiles(const jwstring& folder, jvector<jstring> &files) {
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

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// file is a directory
		} else {
			jstring ext = GetExtA(c);
			if (ext == "dcm" || ext == "raw" || ext == "png")
				files.push_back(GetFullPathA(c));
		}
	} while (FindNextFileA(hFind, &ffd) != 0);

	FindClose(hFind);
}
shared_ptr<Texture> ImageLoader::LoadVolume(const jwstring& path, DirectX::XMFLOAT3 &size) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!\n", path.c_str());
		return nullptr;
	}

	jvector<jstring> files;
	GetFiles(path, files);

	if (files.size() == 0) return nullptr;

	jstring ext = GetExtA(files[0]);
	if (ext == "dcm")
		return LoadDicomVolume(files, size);
	else if (ext == "raw")
		return LoadRawVolume(files, size);
	else
		return nullptr;
}

struct MaskSliceArgs {
	const char* path;
	uint16_t* slice;
	unsigned int width;
	unsigned int height;
};

DWORD WINAPI ProcessMaskSlice(LPVOID lpParam) {
	MaskSliceArgs* args = (MaskSliceArgs*)lpParam;

	std::vector<unsigned char> rgba;
	unsigned int w, h;
	if (lodepng::decode(rgba, w, h, args->path)) {
		OutputDebugf(L"%S: Failed to load PNG!\n", args->path);
		return 0;
	}
	if (w != args->width || h != args->height) {
		OutputDebugf(L"%S: Incorrect dimensions!\n", args->path);
		return 0;
	}

	for (unsigned int x = 0; x < w; x++)
		for (unsigned int y = 0; y < h; y++) {
			unsigned int p = y * w + x;
			args->slice[2 * p + 1] = (uint16_t)((unsigned int)rgba[4 * p] * 257u);
		}

	return 0;
}

void ImageLoader::LoadMask(const jwstring& path, const shared_ptr<Texture>& texture) {
	if (!PathFileExistsW(path.c_str())) {
		OutputDebugf(L"%S Does not exist!\n", path.c_str());
		return;
	}

	jvector<jstring> files;
	GetFiles(path, files);

	if (files.size() != texture->Depth()) {
		OutputDebugf(L"Incorrect slice count! (%d != %d)\n", (int)files.size(), (int)texture->Depth());
		return;
	}

	std::sort(files.data(), files.data() + files.size(), [](const jstring& a, const jstring& b) {
		return atoi(GetNameA(a).c_str()) > atoi(GetNameA(b).c_str());
	});

	jvector<MaskSliceArgs> args;
	args.resize(files.size());

	uint16_t* data = (uint16_t*)texture->GetPixelData();
	unsigned int w = texture->Width();
	unsigned int h = texture->Height();
	for (unsigned int i = 0; i < files.size(); i++) {
		args[i] = { files[i].c_str(), data + 2 * w * h * i, w, h };
		ProcessMaskSlice(&args[i]);
	}

	texture->Upload();
}