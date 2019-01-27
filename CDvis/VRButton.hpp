#pragma once

#include <Object.hpp>
#include <Mesh.hpp>
#include <MeshRenderer.hpp>
#include <Texture.hpp>

class VRButton : public Object {
public:
	VRButton(jwstring name, float size);
	~VRButton();

	void Init(std::shared_ptr<Texture> texture, DirectX::XMFLOAT4 uv_st);

private:
	float mSize;
	std::shared_ptr<MeshRenderer> mRenderer;
};

