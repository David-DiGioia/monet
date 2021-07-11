#pragma once
#include <vector>

#include "glm/glm.hpp"

namespace vkutil {

	glm::mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);

	template<typename T>
	void bufferToPtrArray(std::vector<T*>& ptrArr, std::vector<T>& buf) {
		ptrArr.reserve(buf.size());
		for (T& t : buf) {
			ptrArr.push_back(&t);
		}
	}

}
