#ifndef		HAS_RENDER_TYPES_H_BEEN_INCLUDED
#define		HAS_RENDER_TYPES_H_BEEN_INCLUDED

#include	<glm/glm.hpp>

using		glm::vec4;
using		glm::vec3;

// ----------------------------------------------------------------------------

enum RenderShape
{
	RenderShape_Cube,
	RenderShape_Sphere,
	RenderShape_Line,
	RenderShape_SIZE,

	// None is placed after size to prevent it being picked as a valid value
	RenderShape_None,
};

// ----------------------------------------------------------------------------

struct MeshVertex
{
	MeshVertex()
	{
	}

	MeshVertex(const glm::vec4& pos, const glm::vec4& n, const glm::vec4& c)
		: xyz(pos)
		, normal(n)
		, colour(c)
	{
	}

	glm::vec4			xyz, normal, colour;
};

// ----------------------------------------------------------------------------

struct MeshTriangle
{
	MeshTriangle()
	{
	//	indices_[0] = indices_[1] = indices_[2] = -1;
		indices_[0] = indices_[1] = indices_[2] = 0;
	}

	MeshTriangle(const int i0, const int i1, const int i2)
	{
		indices_[0] = i0;
		indices_[1] = i1;
		indices_[2] = i2;
	}

	int			indices_[3];
};

// ----------------------------------------------------------------------------

#ifdef LEVEN
const int MAX_MESH_VERTICES = 14 * 1024;
#else
const int MAX_MESH_VERTICES = 1 << 23;			// i.e. 8 million
#endif

const int MAX_MESH_TRIANGLES = MAX_MESH_VERTICES * 2;

class MeshBuffer
{
public:

	MeshBuffer()
		: numVertices(0)
		, numTriangles(0)
		, tag(nullptr)
	{
	}

	static void initialiseVertexArray();

	const char*			tag;			

	MeshVertex			vertices[MAX_MESH_VERTICES];
	int					numVertices;

	MeshTriangle		triangles[MAX_MESH_TRIANGLES];
	int					numTriangles;
};


// ----------------------------------------------------------------------------

struct ActorVertex
{
	glm::vec4           pos;
};

struct ActorMeshBuffer
{
	static void initialiseVertexArray();

	ActorVertex*        vertices = nullptr;
	int                 numVertices = 0;

	int*                indices = nullptr;
	int                 numIndices = 0;
};

// ----------------------------------------------------------------------------


#endif	//	HAS_RENDER_TYPES_H_BEEN_INCLUDED
