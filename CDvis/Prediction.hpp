#pragma once

#include <Texture.hpp>

class Prediction {
public:
	static void RunPrediction(const jwstring& dicom, std::shared_ptr<Texture>& volume);
};

