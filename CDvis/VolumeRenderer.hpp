#pragma once

#include <Renderer.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Texture.hpp>

class VolumeRenderer : public Renderer {
public:
	VolumeRenderer();
	VolumeRenderer(jwstring name);
	~VolumeRenderer();
	
	void Draw(std::shared_ptr<CommandList> commandList);

	std::shared_ptr<Texture> mTexture;
	std::shared_ptr<Mesh> mCubeMesh;
	std::shared_ptr<Shader> mShader;

	DirectX::BoundingOrientedBox Bounds();
};

