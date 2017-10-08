#include "render_vertex.h"

void Vertex_SetNormal(GeometryVertex& v, const glm::vec3& normal)
{
	v.n[0] = normal.x;
	v.n[1] = normal.y;
	v.n[2] = normal.z;
	v.n[3] = 0.f;
}

const glm::vec3 Vertex_GetPosition(GeometryVertex& v) 
{
	return glm::vec3(v.p[0], v.p[1], v.p[2]);
}

template <>
void Vertex_SetGLState<BillboardVertex>()
{
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex), (void*)(sizeof(float) * 4));
}

template <>
void Vertex_ResetGLState<BillboardVertex>()
{
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

template <>
void Vertex_SetGLState<GeometryVertex>()
{
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)(sizeof(float) * 4));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)(sizeof(float) * 8));
}

template <>
void Vertex_ResetGLState<GeometryVertex>()
{
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

