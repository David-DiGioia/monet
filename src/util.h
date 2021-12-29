#pragma once
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"
#include "PxPhysicsAPI.h"

namespace vkutil {

	glm::mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);

	template<typename T>
	void bufferToPtrArray(std::vector<T*>& ptrArr, std::vector<T>& buf) {
		ptrArr.reserve(buf.size());
		for (T& t : buf) {
			ptrArr.push_back(&t);
		}
	}

	physx::PxVec3 toPhysx(const glm::vec3& v);

	physx::PxQuat toPhysx(const glm::quat& q);

	glm::vec3 toGLM(const physx::PxVec3& v);

}
