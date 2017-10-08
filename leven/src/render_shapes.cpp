#include	"render_shapes.h"

// for M_PI
#define _USE_MATH_DEFINES
#include	<math.h>

// ----------------------------------------------------------------------------

void CalculateNormals(
	const u16* indices, 
	const int numIndices, 
	const vec4* vertexData, 
	const int numVertices, 
	vec4* normalData)
{
	for (int i = 0; i < numVertices; i++)
	{
		normalData[i] = vec4(0.f);
	}

	for (int i = 0; i < numIndices; i += 3)
	{
		const vec3 p0 = vec3(vertexData[indices[i + 0]]);
		const vec3 p1 = vec3(vertexData[indices[i + 1]]);
		const vec3 p2 = vec3(vertexData[indices[i + 2]]);
		const vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

		normalData[indices[i + 0]] += vec4(normal, 0.f);
		normalData[indices[i + 1]] += vec4(normal, 0.f);
		normalData[indices[i + 2]] += vec4(normal, 0.f);
	}

	for (int i = 0; i < numVertices; i++)
	{
		normalData[i] = glm::normalize(normalData[i]);
	}
}

// ----------------------------------------------------------------------------

const int SPHERE_SUBDIVISIONS = 10;			// how many long/lat subdivisions

void GetSphereData(
	const vec3& origin,
	const float radius,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize)
{
	const int indices[6] = { 0, 2, 1, 2, 3, 1 };

	const vec4 o = vec4(origin, 0.f);
	const vec4 diameter = vec4(radius, radius, radius, 0.f) * 2;

	vec4 vertexPositions[SPHERE_SUBDIVISIONS * SPHERE_SUBDIVISIONS];

	for (int n = 0; n < SPHERE_SUBDIVISIONS; n ++)
	{
		const float theta = (M_PI * n) / (float)SPHERE_SUBDIVISIONS;

		for (int m = 0; m < SPHERE_SUBDIVISIONS; m++)
		{
			const int offset = (n * SPHERE_SUBDIVISIONS) + m;
			const float phi = (2 * M_PI * m) / (float)SPHERE_SUBDIVISIONS;

			const float x = sin(theta) * cos(phi);
			const float y = sin(theta) * sin(phi);
			const float z = cos(theta);
			vertexPositions[offset] = vec4(x, y, z, 0.f);
		}
	}

	int vertexOffset = 0;
	int indexOffset = 0;

	for (int n = 0; n < SPHERE_SUBDIVISIONS; n++)
	for (int m = 0; m < SPHERE_SUBDIVISIONS; m++)
	{
		const int mPlusOne = (m + 1) % SPHERE_SUBDIVISIONS;

		vertexDataBuffer[vertexOffset + 0] = vertexPositions[(n * SPHERE_SUBDIVISIONS) + m];
		vertexDataBuffer[vertexOffset + 1] = vertexPositions[(n * SPHERE_SUBDIVISIONS) + mPlusOne];
		vertexDataBuffer[vertexOffset + 2] = n < (SPHERE_SUBDIVISIONS - 1) ? 
			vertexPositions[((n + 1) * SPHERE_SUBDIVISIONS) + m] : vec4(0.f, 0.f, -1.f, 0.f);
		vertexDataBuffer[vertexOffset + 3] = n < (SPHERE_SUBDIVISIONS - 1) ? 
			vertexPositions[((n + 1) * SPHERE_SUBDIVISIONS) + mPlusOne] : vec4(0.f, 0.f, -1.f, 0.f);

		for (int i = 0; i < 4; i++)
		{
			vertexDataBuffer[vertexOffset + i] = (vertexDataBuffer[vertexOffset + i] * diameter) + o;
		}

		for (int i = 0; i < 6; i++)
		{
			indexDataBuffer[indexOffset + i] = *vertexBufferSize + indices[i];
		}

		*vertexBufferSize += 4;
		*indexBufferSize += 6;

		vertexOffset += 4;
		indexOffset += 6;
	}
}

// ----------------------------------------------------------------------------

void GetSphereDataSizes(int* numVertices, int* numIndices)
{
	LVN_ASSERT(numVertices);
	LVN_ASSERT(numIndices);

	*numVertices = 0;
	*numIndices = 0;

	for (int i = 0; i < SPHERE_SUBDIVISIONS; i++)
	for (int j = 0; j < SPHERE_SUBDIVISIONS; j++)
	{
		*numVertices += 4;
		*numIndices += 6;
	}
}

// ----------------------------------------------------------------------------

void GetCubeData(
	const vec3& min,
	const vec3& max,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize)
{
	vertexDataBuffer[0] = vec4(min.x, min.y, min.z, 0.f);
	vertexDataBuffer[1] = vec4(min.x, max.y, min.z, 0.f);
	vertexDataBuffer[2] = vec4(max.x, max.y, min.z, 0.f);
	vertexDataBuffer[3] = vec4(max.x, min.y, min.z, 0.f);

	vertexDataBuffer[4] = vec4(min.x, min.y, max.z, 0.f);
	vertexDataBuffer[5] = vec4(min.x, max.y, max.z, 0.f);
	vertexDataBuffer[6] = vec4(max.x, max.y, max.z, 0.f);
	vertexDataBuffer[7] = vec4(max.x, min.y, max.z, 0.f);

	const u16 indices[36] = 
	{
		0, 1, 2, 0, 2, 3,
		4, 1, 0, 4, 5, 1,
		0, 3, 4, 3, 7, 4,
		3, 2, 6, 6, 7, 3,
		2, 1, 5, 5, 6, 2,
		4, 6, 5, 4, 7, 6,
	};

	for (int i = 0; i < 36; i++)
	{
		*indexDataBuffer++ = *vertexBufferSize + indices[i];
	}

	*vertexBufferSize += 8;
	*indexBufferSize += 36;
}

// ----------------------------------------------------------------------------

void GetCubeDataSizes(int* numVertices, int* numIndices)
{
	LVN_ASSERT(numVertices);
	LVN_ASSERT(numIndices);

	*numVertices = 8;
	*numIndices = 36;
}

// ----------------------------------------------------------------------------

void GetLineData(
	const vec3& start,
	const vec3& end,
	vec4* vertexDataBuffer,
	u16* indexDataBuffer,
	u32* vertexBufferSize,
	u32* indexBufferSize)
{
	vertexDataBuffer[0] = vec4(start, 0.f);
	vertexDataBuffer[1] = vec4(end, 0.f);

	indexDataBuffer[0] = *vertexBufferSize + 0;
	indexDataBuffer[1] = *vertexBufferSize + 1;

	*vertexBufferSize += 2;
	*indexBufferSize += 2;
}

// ----------------------------------------------------------------------------

void GetLineDataSizes(int* numVertices, int* numIndices)
{
	LVN_ASSERT(numVertices);
	LVN_ASSERT(numIndices);

	*numVertices = 2;
	*numIndices = 2;
}

// ----------------------------------------------------------------------------
