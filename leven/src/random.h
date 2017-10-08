#ifndef     HAS_RANDOM_H_BEEN_INCLUDED
#define     HAS_RANDOM_H_BEEN_INCLUDED

#include	<glm/glm.hpp>

s32 RandomS32(const s32 min, const s32 max);
float RandomF32(const float min, const float max);
glm::vec3 RandomColour();

#endif //   HAS_RANDOM_H_BEEN_INCLUDED
