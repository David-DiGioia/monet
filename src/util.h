#pragma once
#include "glm/glm.hpp"

namespace vkutil {

	glm::mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);

}
