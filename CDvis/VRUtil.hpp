#pragma once

#include <openvr.h>

#include <DirectXMath.h>
#include <Object.hpp>

DirectX::XMFLOAT4X4 VR2DX(const vr::HmdMatrix34_t& mat);
void VR2DX(const vr::HmdMatrix34_t& mat, DirectX::XMFLOAT3 &position, DirectX::XMFLOAT4 &rotation);
void VR2DX(const vr::HmdMatrix34_t& mat, Object* obj);