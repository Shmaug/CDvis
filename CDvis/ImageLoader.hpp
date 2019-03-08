#pragma once

#include <Common.hpp>

// Win32 LoadImage macro
#ifdef LoadImage
#undef LoadImage
#endif

class ImageLoader {
public:
	static std::shared_ptr<Texture> LoadImage(jwstring imagePath, DirectX::XMFLOAT3 &size);
	static std::shared_ptr<Texture> LoadVolume(jwstring folder, DirectX::XMFLOAT3 &size);
};

