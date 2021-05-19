#include "util.h"

glm::mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
	glm::mat4 result{ 1 };
	result[0][0] = 2.0f / (right - left);
	result[1][1] = -2.0f / (top - bottom);
	result[2][2] = -1.0f / (zFar - zNear);
	result[3][0] = -(right + left) / (right - left);
	result[3][1] = -(top + bottom) / (top - bottom);
	result[3][2] = -zNear / (zFar - zNear);
	return result;
}
