#pragma once

#include <Mesh.hpp>
#include <Texture.hpp>
#include <Camera.hpp>
#include <CommandList.hpp>
#include <ConstantBuffer.hpp>
#include <Material.hpp>
#include <MeshRenderer.hpp>

class VRToolTips : public MeshRenderer {
public:
	VRToolTips() : MeshRenderer(L"") {}
	VRToolTips(const jwstring& name);
	~VRToolTips() {}
};

