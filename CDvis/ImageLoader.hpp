#pragma once

#include <Common.hpp>

// Win32 LoadImage macro
#ifdef LoadImage
#undef LoadImage
#endif

class ImageLoader {
public:
	static std::shared_ptr<Texture> LoadImage(const jwstring& imagePath, DirectX::XMFLOAT3 &size);
	static std::shared_ptr<Texture> LoadVolume(const jwstring& folder, DirectX::XMFLOAT3 &size);
	static void LoadMask(const jwstring& folder, const std::shared_ptr<Texture>& texture);
};

