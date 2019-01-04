#pragma once

#include <openvr.h>
#include <DirectXMath.h>
#include <object.hpp>

DirectX::XMFLOAT4X4 VR2DX(vr::HmdMatrix34_t& mat);
void VR2DX(vr::HmdMatrix34_t& mat, Object* obj);