#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <limits>

#include "json.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize.h"

#include "asset_loader.h"
#include "texture_asset.h"
#include "vk_mesh_asset.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"

namespace fs = std::filesystem;
using namespace assets;

struct ConverterState {
	fs::path asset_path;
	fs::path export_path;

	fs::path convertToExportRelative(fs::path path) const;
};

bool convertImage(const fs::path& input, const fs::path& output)
{
	int texWidth, texHeight, texChannels;

	auto pngStart{ std::chrono::high_resolution_clock::now() };

	stbi_uc* pixels{ stbi_load(input.u8string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };

	auto pngEnd{ std::chrono::high_resolution_clock::now() };

	auto diff = pngEnd - pngStart;

	std::cout << "png took " << std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

	if (!pixels) {
		std::cout << "Failed to load texture file " << input << "\n";
		return false;
	}

	int texture_size{ texWidth * texHeight * 4 };

	TextureInfo texinfo;
	texinfo.originalSize = texture_size;


	std::string s{ input.filename().generic_string() };
	bool colorTexture{ s.size() >= 9 && (s.substr(s.size() - 9, 5) == "_diff") };

	texinfo.textureFormat = colorTexture ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;
	texinfo.originalFile = input.string();

	auto mipStart{ std::chrono::high_resolution_clock::now() };

	texinfo.width = texWidth;
	texinfo.height = texHeight;

	std::vector<unsigned char> allBuffer;


	size_t allBufferSize{ (size_t)texture_size };
	uint32_t width{ texinfo.width };
	uint32_t height{ texinfo.height };
	uint32_t miplevels{ 1 };

	// make mipmaps
	while (width > 1 || height > 1) {
		width >>= 1;
		height >>= 1;
		allBufferSize += (size_t)width * height * 4;
		++miplevels;
	}

	texinfo.miplevels = miplevels;
	allBuffer.resize(allBufferSize);
	width = texinfo.width;
	height = texinfo.height;
	size_t mipSize{ (size_t)width * height * 4 };
	unsigned char* ptr{ allBuffer.data() };

	memcpy(ptr, pixels, mipSize);

	while (mipSize > 0) {
		stbir_resize_uint8(ptr, width, height, 0, ptr + mipSize, width / 2, height / 2, 0, 4);
		ptr += mipSize;
		width >>= 1;
		height >>= 1;
		mipSize = (size_t)width * height * 4;
	}

	auto mipEnd{ std::chrono::high_resolution_clock::now() };
	auto mipDiff{ mipEnd - mipStart };

	std::cout << "creating mipmaps took " << std::chrono::duration_cast<std::chrono::nanoseconds>(mipDiff).count() / 1000000.0 << "ms" << std::endl;

	texinfo.originalSize = allBuffer.size();
	assets::AssetFile newImage{ assets::packTexture(&texinfo, allBuffer.data()) };

	nlohmann::json textureMetadata;
	textureMetadata["format"] = "RGBA8";
	textureMetadata["original_size"] = texinfo.originalSize;
	textureMetadata["original_file"] = texinfo.originalFile;
	textureMetadata["miplevels"] = texinfo.miplevels;
	textureMetadata["width"] = texinfo.width;
	textureMetadata["height"] = texinfo.height;

	stbi_image_free(pixels);

	// will write compression_mode field of textureMetadata, and write that to newImage before saving
	saveBinaryFile(output.string().c_str(), textureMetadata, newImage);

	return true;
}

void unpackBufferGLTF(tinygltf::Model& model, tinygltf::Accessor& accesor, std::vector<uint8_t>& outputBuffer)
{
	int bufferID = accesor.bufferView;
	size_t elementSize = tinygltf::GetComponentSizeInBytes(accesor.componentType);

	tinygltf::BufferView& bufferView = model.bufferViews[bufferID];

	tinygltf::Buffer& bufferData = (model.buffers[bufferView.buffer]);


	uint8_t* dataptr = bufferData.data.data() + accesor.byteOffset + bufferView.byteOffset;

	int components = tinygltf::GetNumComponentsInType(accesor.type);

	elementSize *= components;

	size_t stride = bufferView.byteStride;
	if (stride == 0)
	{
		stride = elementSize;

	}

	outputBuffer.resize(accesor.count * elementSize);

	for (int i = 0; i < accesor.count; i++) {
		uint8_t* dataindex = dataptr + stride * i;
		uint8_t* targetptr = outputBuffer.data() + elementSize * i;

		memcpy(targetptr, dataindex, elementSize);
	}
}

// implementation for extractVerticesGLTF. This allows specialization to work properly for skinning
template <typename T>
void extractVerticesInternalGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<T>& _vertices)
{
	tinygltf::Accessor& pos_accesor = model.accessors[primitive.attributes["POSITION"]];
	_vertices.resize(pos_accesor.count);
	std::vector<uint8_t> pos_data;
	unpackBufferGLTF(model, pos_accesor, pos_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (pos_accesor.type == TINYGLTF_TYPE_VEC3)
		{
			if (pos_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)pos_data.data();

				//vec3f 
				_vertices[i].position[0] = *(dtf + (i * 3) + 0);
				_vertices[i].position[1] = *(dtf + (i * 3) + 1);
				_vertices[i].position[2] = *(dtf + (i * 3) + 2);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& normal_accesor = model.accessors[primitive.attributes["NORMAL"]];
	std::vector<uint8_t> normal_data;
	unpackBufferGLTF(model, normal_accesor, normal_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (normal_accesor.type == TINYGLTF_TYPE_VEC3)
		{
			if (normal_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)normal_data.data();

				//vec3f 
				_vertices[i].normal[0] = *(dtf + (i * 3) + 0);
				_vertices[i].normal[1] = *(dtf + (i * 3) + 1);
				_vertices[i].normal[2] = *(dtf + (i * 3) + 2);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& tangent_accesor = model.accessors[primitive.attributes["TANGENT"]];
	std::vector<uint8_t> tangent_data;
	unpackBufferGLTF(model, tangent_accesor, tangent_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (tangent_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (tangent_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)tangent_data.data();

				_vertices[i].tangent[0] = *(dtf + (i * 4) + 0);
				_vertices[i].tangent[1] = *(dtf + (i * 4) + 1);
				_vertices[i].tangent[2] = *(dtf + (i * 4) + 2);
				_vertices[i].tangent[3] = *(dtf + (i * 4) + 3);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& uv_accesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
	std::vector<uint8_t> uv_data;
	unpackBufferGLTF(model, uv_accesor, uv_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (uv_accesor.type == TINYGLTF_TYPE_VEC2)
		{
			if (uv_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)uv_data.data();

				//vec3f 
				_vertices[i].uv[0] = *(dtf + (i * 2) + 0);
				_vertices[i].uv[1] = *(dtf + (i * 2) + 1);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	return;
}
// Vertex with tangent attribute
template <typename T>
void extractVerticesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<T>& _vertices)
{
	extractVerticesInternalGLTF(primitive, model, _vertices);
}

// specialization for skinned vertices
template <>
void extractVerticesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<VertexSkinned>& _vertices)
{
	extractVerticesInternalGLTF(primitive, model, _vertices);

	tinygltf::Accessor& joints_accesor = model.accessors[primitive.attributes["JOINTS_0"]];
	std::vector<uint8_t> joints_data;
	unpackBufferGLTF(model, joints_accesor, joints_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (joints_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (joints_accesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				uint8_t* dtf = (uint8_t*)joints_data.data();

				for (int j = 0; j < 4; ++j) {
					auto jointIndex{ *(dtf + (i * 4) + j) };
					//_vertices[i].jointIndices[j] = model.skins[0].joints[jointIndex];
					_vertices[i].jointIndices[j] = jointIndex;
				}

			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& weights_accesor = model.accessors[primitive.attributes["WEIGHTS_0"]];
	std::vector<uint8_t> weights_data;
	unpackBufferGLTF(model, weights_accesor, weights_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (weights_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (weights_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)weights_data.data();

				_vertices[i].jointWeights[0] = *(dtf + (i * 4) + 0);
				_vertices[i].jointWeights[1] = *(dtf + (i * 4) + 1);
				_vertices[i].jointWeights[2] = *(dtf + (i * 4) + 2);
				_vertices[i].jointWeights[3] = *(dtf + (i * 4) + 3);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}
}

void extractIndicesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<uint16_t>& _primindices)
{
	int indexaccesor = primitive.indices;

	int indexbuffer = model.accessors[indexaccesor].bufferView;
	int componentType = model.accessors[indexaccesor].componentType;
	size_t indexsize = tinygltf::GetComponentSizeInBytes(componentType);

	tinygltf::BufferView& indexview = model.bufferViews[indexbuffer];
	int bufferidx = indexview.buffer;

	tinygltf::Buffer& buffindex = (model.buffers[bufferidx]);

	uint8_t* dataptr = buffindex.data.data() + indexview.byteOffset;

	std::vector<uint8_t> unpackedIndices;
	unpackBufferGLTF(model, model.accessors[indexaccesor], unpackedIndices);

	for (int i = 0; i < model.accessors[indexaccesor].count; i++) {

		uint32_t index;
		switch (componentType) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t* bfr = (uint16_t*)unpackedIndices.data();
			index = *(bfr + i);
		}
		break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			int16_t* bfr = (int16_t*)unpackedIndices.data();
			index = *(bfr + i);
		}
		break;
		default:
			assert(false);
		}

		_primindices.push_back(index);
	}

	for (int i = 0; i < _primindices.size() / 3; i++)
	{
		//flip the triangle

		std::swap(_primindices[i * 3 + 1], _primindices[i * 3 + 2]);
	}
}

std::string calculateMeshNameGLTF(tinygltf::Model& model, int meshIndex, int primitiveIndex)
{
	char buffer0[50];
	char buffer1[50];
	itoa(meshIndex, buffer0, 10);
	itoa(primitiveIndex, buffer1, 10);

	std::string meshname = "MESH_" + std::string{ &buffer0[0] } + "_" + model.meshes[meshIndex].name;

	bool multiprim = model.meshes[meshIndex].primitives.size() > 1;

	if (multiprim) {
		meshname += "_PRIM_" + std::string{ &buffer1[0] };
	}

	return meshname;
}

std::string calculateSkeletonNameGLTF()
{
	return std::string{ "SKEL" };
}

template <typename VFormat>
bool extractMeshesGLTF(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState, VertexFormat vertexFormatEnum)
{
	/*
		Note: meshes are what we normally think of as meshes, but primitives do NOT refer to triangles in this case.
		Primitives are a single mesh broken up into pieces, either to allow a single mesh to have multiple materials
		or to avoid the limitation of 65535 vertices (for 16-bit indices) for a mesh.
	*/

	tinygltf::Model* glmod = &model;
	for (auto meshindex = 0; meshindex < model.meshes.size(); ++meshindex) {

		auto& glmesh = model.meshes[meshindex];

		//using Format = assets::Vertex_f32_PNTV;
		//auto vertexFormatEnum = assets::VertexFormat::PNTV_F32;

		std::vector<VFormat> _vertices;
		std::vector<uint16_t> _indices;

		for (auto primindex = 0; primindex < glmesh.primitives.size(); ++primindex) {

			_vertices.clear();
			_indices.clear();

			std::string meshname{ calculateMeshNameGLTF(model, meshindex, primindex) };

			tinygltf::Primitive& primitive = glmesh.primitives[primindex];

			extractIndicesGLTF(primitive, model, _indices);
			extractVerticesGLTF(primitive, model, _vertices);


			MeshInfo meshinfo;
			meshinfo.vertexFormat = vertexFormatEnum;
			meshinfo.vertexBufferSize = _vertices.size() * sizeof(VFormat);
			meshinfo.indexBufferSize = _indices.size() * sizeof(uint16_t);
			meshinfo.indexSize = sizeof(uint16_t);
			meshinfo.originalFile = input.string();

			meshinfo.bounds = calculateBounds(_vertices.data(), _vertices.size());

			assets::AssetFile newFile{ packMesh(&meshinfo, (char*)_vertices.data(), (char*)_indices.data()) };

			nlohmann::json metadata;

			if (meshinfo.vertexFormat == VertexFormat::DEFAULT) {
				metadata["vertex_format"] = "DEFAULT";
			} else if (meshinfo.vertexFormat == VertexFormat::SKINNED) {
				metadata["vertex_format"] = "SKINNED";
			}

			metadata["vertex_buffer_size"] = meshinfo.vertexBufferSize;
			metadata["index_buffer_size"] = meshinfo.indexBufferSize;
			metadata["index_size"] = meshinfo.indexSize;
			metadata["original_file"] = meshinfo.originalFile;

			std::vector<float> boundsData;
			boundsData.resize(7);

			boundsData[0] = meshinfo.bounds.origin[0];
			boundsData[1] = meshinfo.bounds.origin[1];
			boundsData[2] = meshinfo.bounds.origin[2];

			boundsData[3] = meshinfo.bounds.radius;

			boundsData[4] = meshinfo.bounds.extents[0];
			boundsData[5] = meshinfo.bounds.extents[1];
			boundsData[6] = meshinfo.bounds.extents[2];

			metadata["bounds"] = boundsData;

			fs::path meshpath = outputFolder / (meshname + ".mesh");

			//save to disk
			saveBinaryFile(meshpath.string().c_str(), metadata, newFile);
		}
	}
	return true;
}

int getSkeletonRootIdx(const std::vector<NodeAsset>& nodes, const std::vector<int>& joints)
{
	if (joints.empty()) {
		std::cout << "Unexpected: joints vector is empty in getSkeletonRootIdx()\n";
		return -1;
	}

	std::unordered_set<int> jointsSet{ joints.begin(), joints.end() };

	int joint{ joints[0] };
	int prev{ -1 };

	while (jointsSet.find(joint) != jointsSet.end()) {
		prev = joint;
		joint = nodes[joint].parentIdx;
	}

	return prev;
}

// from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.cpp
void loadSkins(SkeletalAnimationDataAsset& data, tinygltf::Model& gltfModel, int32_t meshNodeIdx)
{

	if (gltfModel.skins.size() > 1) {
		std::cout << "Error: More than one skin not supported yet!\n";
	}

	for (tinygltf::Skin& source : gltfModel.skins) {
		SkinAsset newSkin{};
		newSkin.name = source.name;
		// turns out, skeletonRootIdx is not mandatory for gltf files so we will calculate it ourself...
		//newSkin.skeletonRootIdx = source.skeleton;
		newSkin.meshNodeIdx = meshNodeIdx;

		// Find joint nodes
		for (int jointIndex : source.joints) {
			if (jointIndex > -1) {
				newSkin.joints.push_back(jointIndex);
			}
		}

		newSkin.skeletonRootIdx = getSkeletonRootIdx(data.nodes, newSkin.joints);

		// Get inverse bind matrices from buffer
		if (source.inverseBindMatrices > -1) {
			const tinygltf::Accessor& accessor = gltfModel.accessors[source.inverseBindMatrices];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
			newSkin.inverseBindMatrices.resize(accessor.count);
			memcpy(newSkin.inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
		}

		data.skins.push_back(newSkin);
	}
}

void loadAnimations(SkeletalAnimationDataAsset& data, tinygltf::Model& gltfModel)
{
	for (tinygltf::Animation& anim : gltfModel.animations) {
		Animation animation{};
		animation.name = anim.name;
		if (anim.name.empty()) {
			animation.name = std::to_string(data.animations.size());
		}

		// Samplers
		for (auto& samp : anim.samplers) {
			AnimationSampler sampler{};

			if (samp.interpolation == "LINEAR") {
				sampler.interpolation = AnimationSampler::InterpolationType::LINEAR;
			}
			if (samp.interpolation == "STEP") {
				sampler.interpolation = AnimationSampler::InterpolationType::STEP;
			}
			if (samp.interpolation == "CUBICSPLINE") {
				sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
			}

			// Read sampler input time values
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[samp.input];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
				const float* buf = static_cast<const float*>(dataPtr);
				for (size_t index = 0; index < accessor.count; index++) {
					sampler.inputs.push_back(buf[index]);
				}

				for (auto input : sampler.inputs) {
					if (input < animation.start) {
						animation.start = input;
					};
					if (input > animation.end) {
						animation.end = input;
					}
				}
			}

			// Read sampler output T/R/S values 
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[samp.output];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				switch (accessor.type) {
				case TINYGLTF_TYPE_VEC3: {
					const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
					}
					break;
				}
				case TINYGLTF_TYPE_VEC4: {
					const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						sampler.outputsVec4.push_back(buf[index]);
					}
					break;
				}
				default: {
					std::cout << "unknown type" << std::endl;
					break;
				}
				}
			}

			animation.samplers.push_back(sampler);
		}

		// Channels
		for (tinygltf::AnimationChannel& source : anim.channels) {
			AnimationChannel channel{};

			if (source.target_path == "rotation") {
				channel.path = AnimationChannel::PathType::ROTATION;
			}
			if (source.target_path == "translation") {
				channel.path = AnimationChannel::PathType::TRANSLATION;
			}
			if (source.target_path == "scale") {
				channel.path = AnimationChannel::PathType::SCALE;
			}
			if (source.target_path == "weights") {
				std::cout << "weights not yet supported, skipping channel\n";
				continue;
			}
			channel.samplerIndex = source.sampler;
			channel.nodeIdx = source.target_node;
			if (channel.nodeIdx < 0) {
				continue;
			}

			animation.channels.push_back(channel);
		}

		data.animations.push_back(animation);
	}
}

void loadNode(SkeletalAnimationDataAsset& data, const tinygltf::Node& node, const tinygltf::Model& model)
{
	// parent index is set by caller after all nodes are loaded
	NodeAsset newNode{};
	newNode.parentIdx = -1;
	newNode.name = node.name;
	//newNode.skinIndex = node.skin;
	newNode.matrix = glm::mat4(1.0f);
	newNode.mesh = node.mesh;

	// Generate local node matrix
	glm::vec3 translation = glm::vec3(0.0f);
	if (node.translation.size() == 3) {
		translation = glm::make_vec3(node.translation.data());
		newNode.translation = translation;
	}
	glm::mat4 rotation = glm::mat4(1.0f);
	if (node.rotation.size() == 4) {
		glm::quat q = glm::make_quat(node.rotation.data());
		newNode.rotation = glm::mat4(q);
	}
	glm::vec3 scale = glm::vec3(1.0f);
	if (node.scale.size() == 3) {
		scale = glm::make_vec3(node.scale.data());
		newNode.scale = scale;
	}
	if (node.matrix.size() == 16) {
		newNode.matrix = glm::make_mat4x4(node.matrix.data());
	};

	// Node with children
	for (int child : node.children) {
		newNode.children.push_back(child);
	}

	data.nodes.push_back(newNode);
	data.linearNodes.push_back(newNode);
}

void extractSkeletalAnimation(tinygltf::Model gltfModel, const fs::path& input, const fs::path& outputFolder)
{
	std::string error;

	SkeletalAnimationDataAsset data{};

	const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

	int32_t meshNodeIdx{ -1 };
	for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {

		std::cout << gltfModel.nodes[i].name << '\n';

		loadNode(data, gltfModel.nodes[i], gltfModel);
		if (data.nodes.back().mesh > -1) {
			meshNodeIdx = i;
		}
	}

	// set parent indices for loaded nodes
	for (int i = 0; i < data.nodes.size(); ++i) {
		for (int32_t child : data.nodes[i].children) {
			data.nodes[child].parentIdx = i;
		}
	}

	if (gltfModel.animations.size() > 0) {
		loadAnimations(data, gltfModel);
	}

	loadSkins(data, gltfModel, meshNodeIdx);

	// serialize
	std::string skelName{ calculateSkeletonNameGLTF() };
	fs::path skelPath = outputFolder / (skelName + ".skel");

	{
		std::ofstream ofs{ skelPath, std::ios::binary };
		cereal::BinaryOutputArchive oarchive(ofs);

		oarchive(data);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cout << "You need to put the path to the info file";
		return -1;
	} else {

		fs::path path{ argv[1] };

		fs::path directory = path;

		fs::path exported_dir = path.parent_path() / "assets_export";

		std::cout << "loaded asset directory at " << directory << std::endl;

		ConverterState convstate;
		convstate.asset_path = path;
		convstate.export_path = exported_dir;

		for (auto& p : fs::recursive_directory_iterator(directory)) {
			std::cout << "File: " << p << std::endl;

			auto relative = p.path().lexically_proximate(directory);

			auto export_path = exported_dir / relative;

			if (!fs::is_directory(export_path.parent_path())) {
				fs::create_directory(export_path.parent_path());
			}

			if (p.path().extension() == ".png" || p.path().extension() == ".jpg" || p.path().extension() == ".TGA") {
				std::cout << "found a texture" << std::endl;

				auto newpath = p.path();

				export_path.replace_extension(".tx");

				convertImage(p.path(), export_path);
			}

			if (p.path().extension() == ".gltf") {
				std::cout << "found a mesh (gltf)\n";

				tinygltf::Model model;
				tinygltf::TinyGLTF loader;
				std::string err;
				std::string warn;

				bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, p.path().string().c_str());

				if (!warn.empty()) {
					printf("Warn: %s\n", warn.c_str());
				}

				if (!err.empty()) {
					printf("Err: %s\n", err.c_str());
				}

				if (!ret) {
					printf("Failed to parse glTF\n");
					return -1;
				} else {
					auto folder = export_path.parent_path() / (p.path().stem().string() + "_GLTF");
					fs::create_directory(folder);

					// If the mesh is skinned, we must use vertex format which includes skinning data
					std::cout << "skins: " << model.skins.size() << '\n';
					if (model.skins.size() == 0) {
						extractMeshesGLTF<Vertex>(model, p.path(), folder, convstate, VertexFormat::DEFAULT);
					} else {
						extractMeshesGLTF<VertexSkinned>(model, p.path(), folder, convstate, VertexFormat::SKINNED);
						extractSkeletalAnimation(model, p.path(), folder);
					}

					//extractMaterialsGLTF(model, p.path(), folder, convstate);
				}
			}
		}
	}

	return 0;
}

fs::path ConverterState::convertToExportRelative(fs::path path) const
{
	return path.lexically_proximate(export_path);
}