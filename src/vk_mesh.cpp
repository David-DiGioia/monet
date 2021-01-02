#include "vk_mesh.h"

#include <cstddef> // offsetof
#include <iostream>

#include "tiny_obj_loader.h"

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	// we will have just 1 vertex buffer binding, with a per-vertex rate
	VkVertexInputBindingDescription mainBinding{};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	VkVertexInputAttributeDescription positionAttribute{};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute{};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	normalAttribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription tangentAttribute{};
	tangentAttribute.binding = 0;
	tangentAttribute.location = 2;
	tangentAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	tangentAttribute.offset = offsetof(Vertex, tangent);

	VkVertexInputAttributeDescription uvAttribute{};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT; // vec2
	uvAttribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(tangentAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}

bool Mesh::load_from_obj(const std::string& filename)
{
	// attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	// shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	// materials contain the information about the material of each shape, but we won't use it
	std::vector<tinyobj::material_t> materials;

	// error and warning output from the load function
	std::string warn;
	std::string err;

	// load the OBJ file
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), (filename + "/..").c_str());
	// make sure to output the warnings to the console in case there are issues with the file
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << '\n';
	}
	// if we have any error, print it to the console, and break the mesh loading.
	// This happens if the file can't be found or is malformed
	if (!err.empty()) {
		std::cerr << err << '\n';
		return false;
	}

	for (size_t s{ 0 }; s < shapes.size(); ++s) {
		// loop over faces
		size_t index_offset{ 0 };
		for (size_t f{ 0 }; f < shapes[s].mesh.num_face_vertices.size(); ++f) {

			// hardcode loading to triangles
			int fv{ 3 };

			// loop over vertices in the face
			for (size_t v{ 0 }; v < fv; ++v) {
				// access to vertex
				tinyobj::index_t idx{ shapes[s].mesh.indices[index_offset + v] };

				// vertex position
				tinyobj::real_t vx{ attrib.vertices[3 * (uint64_t)idx.vertex_index + 0] };
				tinyobj::real_t vy{ attrib.vertices[3 * (uint64_t)idx.vertex_index + 1] };
				tinyobj::real_t vz{ attrib.vertices[3 * (uint64_t)idx.vertex_index + 2] };

				// vertex normal
				tinyobj::real_t nx{ attrib.normals[3 * (uint64_t)idx.normal_index + 0] };
				tinyobj::real_t ny{ attrib.normals[3 * (uint64_t)idx.normal_index + 1] };
				tinyobj::real_t nz{ attrib.normals[3 * (uint64_t)idx.normal_index + 2] };

				// vertex uv
				tinyobj::real_t ux{ attrib.texcoords[2 * (uint64_t)idx.texcoord_index + 0] };
				tinyobj::real_t uy{ attrib.texcoords[2 * (uint64_t)idx.texcoord_index + 1] };

				// copy it into our vertex
				Vertex new_vert{};
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;

				new_vert.uv.x = ux;
				new_vert.uv.y = 1.0f - uy; // Vulkan UV coordinates are flipped about y-axis

				_vertices.push_back(new_vert);
			}

			// load tangent vectors
			glm::vec3 pos1{ _vertices[(_vertices.size() - 1) - 2].position };
			glm::vec3 pos2{ _vertices[(_vertices.size() - 1) - 1].position };
			glm::vec3 pos3{ _vertices[(_vertices.size() - 1) - 0].position };

			glm::vec2 uv1{ _vertices[(_vertices.size() - 1) - 2].uv };
			glm::vec2 uv2{ _vertices[(_vertices.size() - 1) - 1].uv };
			glm::vec2 uv3{ _vertices[(_vertices.size() - 1) - 0].uv };

			glm::vec3 n{ _vertices[_vertices.size() - 1].normal };

			glm::vec3 edge1{ pos2 - pos1 };
			glm::vec3 edge2{ pos3 - pos1 };
			glm::vec2 deltaUV1{ uv2 - uv1 };
			glm::vec2 deltaUV2{ uv3 - uv1 };

			float inv{ 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y) };

			glm::vec3 tangent{
				inv * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
				inv * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
				inv * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
			};

			_vertices[(_vertices.size() - 1) - 2].tangent = tangent;
			_vertices[(_vertices.size() - 1) - 1].tangent = tangent;
			_vertices[(_vertices.size() - 1) - 0].tangent = tangent;

			index_offset += fv;
		}
	}
	return true;
}
