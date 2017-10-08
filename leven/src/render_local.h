#ifndef		HAS_RENDER_LOCAL_H_BEEN_INCLUDED
#define		HAS_RENDER_LOCAL_H_BEEN_INCLUDED

#include	<functional>
#include	<vector>

#include	"render.h"

// ----------------------------------------------------------------------------

typedef std::function<bool(void)> RenderCommand;
void PushRenderCommand(RenderCommand cmd);

// ----------------------------------------------------------------------------

struct DebugDrawBuffer
{
	RenderShape shape = RenderShape_Cube;			// TODO broken need to support multiple shapes per buffer
	GLuint		vertexBuffer = 0;
	GLuint		colourBuffer = 0;
	GLuint		indexBuffer = 0;
	GLuint		vao = 0;
	u32			numVertices = 0;
	u32			numIndices = 0;
};

void InitialiseDebugDraw();
void DestroyDebugDraw();
void DrawDebugBuffers(const glm::mat4& projection, const glm::mat4& worldView);

// ----------------------------------------------------------------------------

void InitialiseRenderMesh();
void DestroyRenderMesh();

// ----------------------------------------------------------------------------


#endif	//	HAS_RENDER_LOCAL_H_BEEN_INCLUDED
