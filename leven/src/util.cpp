#include	"util.h"

#include	<stdlib.h>

const glm::ivec3 RoundVec3(const glm::vec3& v)
{
	return glm::ivec3(
		floorf(0.5f + v.x),
		floorf(0.5f + v.y),
		floorf(0.5f + v.z)
	);
}
