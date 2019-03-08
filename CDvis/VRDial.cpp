#include "VRDial.hpp"

#include <AssetDatabase.hpp>
#include <Mesh.hpp>
#include <Texture.hpp>

#include "VRDevice.hpp"

using namespace std;

VRDial::VRDial(jwstring name) : MeshRenderer(name) {
	auto dialMesh = AssetDatabase::GetAsset<Mesh>(L"dial");
	dialMesh->UploadStatic();
	auto dialTex = AssetDatabase::GetAsset<Texture>(L"dial_diffuse");
	dialTex->Upload();
}
VRDial::~VRDial() {}

void VRDial::DragStart(const shared_ptr<VRDevice>& device) {

}
void VRDial::DragStop(const shared_ptr<VRDevice>& device) {

}
