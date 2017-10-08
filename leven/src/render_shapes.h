#ifndef		HAS_RENDER_SHAPES_H_BEEN_INCLUDED
#define		HAS_RENDER_SHAPES_H_BEEN_INCLUDED

#include	"render_types.h"

#include	<vector>
#include	<glm/glm.hpp>

using glm::vec4;
using glm::vec3;

// ----------------------------------------------------------------------------

void GetSphereData(
	const vec3& origin,
	const float radius,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize);

void GetCubeData(
	const vec3& min,
	const vec3& max,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize);

void GetLineData(
	const vec3& start,
	const vec3& end,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize);

void GetShapeData(const RenderShape shape, std::vector<vec4>& vertexData, std::vector<int>& indices);

// ----------------------------------------------------------------------------

void GetCubeDataSizes(int* numVertices, int* numIndices);
void GetSphereDataSizes(int* numVertices, int* numIndices);
void GetLineDataSizes(int* numVertices, int* numIndices);

// ----------------------------------------------------------------------------

#endif	//	HAS_RENDER_SHAPES_H_BEEN_INCLUDED
