#pragma once
#include "glm/glm.hpp"

namespace eutil {

	glm::vec3 facing(const glm::mat4& rot);

	glm::vec2 facing2d(const glm::mat4& rot);

}
