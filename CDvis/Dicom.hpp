#pragma once

#include <Common.hpp>

// Win32 LoadImage macro
#ifdef LoadImage
#undef LoadImage
#endif

class Dicom {
public:
	static std::shared_ptr<Texture> LoadImage(jwstring imagePath);
	static std::shared_ptr<Texture> LoadVolume(jwstring folder);
};

