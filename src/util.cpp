#include "util.h"

/*
* Derivation:
* since -z is in front of camera and depth is in range [0, 1]
* 
* We have ax + b.
* m is the far plane.
* n is the near plane.
* The far plane should map to a depth of 1, and the near plane to a depth of 0.
* So then:
* 
* -am + b = 1
* -an + b = 0
* 
* -am + an = 1
* -a(m-n) = 1
* a = -1 / (m-n)
* 
* b = an = -n / (m-n)
*/
glm::mat4 vkutil::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
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
