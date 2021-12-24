#include "engine_util.h"

glm::vec3 eutil::facing(const glm::mat4& rot)
{
	glm::vec4 v{ rot * glm::vec4{0.0f, 0.0f, 1.0f, 0.0f} };
	return glm::normalize(glm::vec3{ v.x, v.y, v.z });
}

glm::vec2 eutil::facing2d(const glm::mat4& rot)
{
	glm::vec4 v{ rot * glm::vec4{0.0f, 0.0f, 1.0f, 0.0f} };
	return glm::normalize(glm::vec2{ v.x, v.z });
}
