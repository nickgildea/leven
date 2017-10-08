#ifndef		__RENDER_VERTEX_H__
#define		__RENDER_VERTEX_H__

#if 0

#include <GL/glew.h>
#include <SDL.h>
#include <GL/GL.h>
#include <GL/GLu.h>
#include <glm/glm.hpp>

typedef unsigned short Index;

struct BillboardVertex
{
	BillboardVertex()
	{
		p[0] = p[1] = p[2] = 0.f; p[3] = 1.f;
		c[0] = c[1] = c[2] = 1.f; c[3] = 0.3f;
	}

	BillboardVertex(const BillboardVertex& other)
	{
		p[0] = other.p[0];
		p[1] = other.p[1];
		p[2] = other.p[2];
		p[3] = other.p[3];

		c[0] = other.c[0];
		c[1] = other.c[1];
		c[2] = other.c[2];
		c[3] = other.c[3];
	}

	BillboardVertex(float pX, float pY, float pZ, float cR, float cG, float cB)
	{
		p[0] = pX;
		p[1] = pY;
		p[2] = pZ;
		p[3] = 1.f;

		c[0] = cR;
		c[1] = cG;
		c[2] = cB; 
		c[3] = 0.3f;
	}

	float		p[4];
	float		c[4];
};


struct GeometryVertex
{
	GeometryVertex(const GeometryVertex& other)
	{
		p[0] = other.p[0];
		p[1] = other.p[1];
		p[2] = other.p[2];
		p[3] = other.p[3];

		n[0] = other.n[0];
		n[1] = other.n[1];
		n[2] = other.n[2];
		n[3] = other.n[3];

		st[0] = other.st[0];
		st[1] = other.st[1];
	}

	GeometryVertex()
	{
		p[0] = p[1] = p[2] = p[3] = 0.f;
		n[0] = n[1] = n[2] = n[3] = 0.f;
		st[0] = st[1] = 0.f;
	}

	GeometryVertex(float pX, float pY, float pZ, float pW=1.f)
	{
		p[0] = pX;
		p[1] = pY;
		p[2] = pZ;
		p[3] = pW;

		n[0] = n[1] = n[2] = n[3] = 0.f;
		st[0] = st[1] = 0.f;

		// TODO remove me
		int *p = 0; *p = 0;
	}

	GeometryVertex(float pX, float pY, float pZ,
		   float s, float t)
	{
		p[0] = pX;
		p[1] = pY;
		p[2] = pZ;
		p[3] = 1.f;

		st[0] = s;
		st[1] = t;

		n[0] = n[1] = n[2] = n[3] = 0.f;
	}
	
	GeometryVertex(float pX, float pY, float pZ,
		   float nX, float nY, float nZ)
	{
		p[0] = pX;
		p[1] = pY;
		p[2] = pZ;
		p[3] = 1.f;

		n[0] = nX;
		n[1] = nY;
		n[2] = nZ;
		n[3] = 1.f;

		st[0] = st[1] = 0.f;
	}

	GeometryVertex(float pX, float pY, float pZ, float pW,
		   float nX, float nY, float nZ, float nW)
	{
		p[0] = pX;
		p[1] = pY;
		p[2] = pZ;
		p[3] = pW;

		n[0] = nX;
		n[1] = nY;
		n[2] = nZ;
		n[3] = nW;

		st[0] = st[1] = 0.f;
	}

	float	p[4];
	float	n[4];
	float	st[2];
};


void Vertex_SetNormal(GeometryVertex& v, const glm::vec3& normal);
const glm::vec3 Vertex_GetPosition(GeometryVertex& v);

template <typename T> void Vertex_SetGLState();
template <typename T> void Vertex_ResetGLState();

template <typename VertexType>
struct VertexBuffer
{
	VertexBuffer()
		: vertex_(0)
	{
		glGenVertexArrays(1, &arrayObj_);
		glGenBuffers(1, &vertex_);
		glGenBuffers(1, &index_);
	}

	~VertexBuffer()
	{
		glDeleteBuffers(1, &index_);
		glDeleteBuffers(1, &vertex_);
		glDeleteVertexArrays(1, &arrayObj_);
	}

	void setData(const VertexType* vertices, size_t vertexDataSize, const Index* indices, size_t indexDataSize)
	{
		glBindVertexArray(arrayObj_);

		glBindBuffer(GL_ARRAY_BUFFER, vertex_);
		glBufferData(GL_ARRAY_BUFFER, vertexDataSize, (float*)(&vertices[0].p), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataSize, (Index*)(&indices[0]), GL_STATIC_DRAW);

		Vertex_SetGLState<VertexType>();

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		Vertex_ResetGLState<VertexType>();
	}

	struct View
	{
		View(const VertexBuffer<VertexType>& buffer)
		{
			// note the ordering here matters, we need to bind a buffer first
			glBindVertexArray(buffer.arrayObj_);
		}

		~View()
		{
			glBindVertexArray(0);
		}
	};

	GLuint arrayObj_;
	GLuint vertex_;
	GLuint index_;
};


#endif

#endif	//	__RENDER_VERTEX_H__

