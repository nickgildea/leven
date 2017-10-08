#ifndef		HAS_RENDER_MESH_H_BEEN_INCLUDED
#define		HAS_RENDER_MESH_H_BEEN_INCLUDED

#include	"render_types.h"
#include	"render_program.h"

#include	"sdl_wrapper.h"
#include	<glm/glm.hpp>
#include	<Remotery.h>

using		glm::vec3;
using 		glm::vec4;
using		glm::mat3;

// ----------------------------------------------------------------------------

MeshBuffer* Render_AllocMeshBuffer(const char* const tag);
void Render_FreeMeshBuffer(MeshBuffer* buffer);

// ----------------------------------------------------------------------------

class RenderMesh;

RenderMesh* Render_AllocRenderMesh(const char* const tag, MeshBuffer* buffer, const glm::vec3& position);
RenderMesh* Render_AllocRenderMesh(const char* const tag, ActorMeshBuffer* buffer);
void Render_FreeRenderMesh(RenderMesh** mesh);

// ----------------------------------------------------------------------------

class RenderMesh
{
public:

	typedef void (*InitVertexArrayFn)(void);

	RenderMesh() {}
	
	// ----------------------------------------------------------------------------

	void destroy()
	{
		if (vao_ == 0)
		{
			// not initialised / already destroy
			return;
		}

		numTriangles_ = numVertices_ = 0;
		position_ = vec3(0.f);
		transform_ = mat3(1.f);

		if (ibuffer_) { glDeleteBuffers(1, &ibuffer_); }
		if (vbuffer_) { glDeleteBuffers(1, &vbuffer_); }
		if (vao_) { glDeleteVertexArrays(1, &vao_); }

		ibuffer_ = vbuffer_ = vao_ = 0;
	}

	// ----------------------------------------------------------------------------

	const int numVertices() const { return numVertices_; }

	// ----------------------------------------------------------------------------

	const int numTriangles() const { return numTriangles_; }

	// ----------------------------------------------------------------------------

	// TODO remove, hardly used
	void setPosition(const vec3& worldspacePosition) { position_ = worldspacePosition; }
	const vec3& getPosition() const { return position_; }

	void setTransform(const mat3& t) { transform_ = t; }

	void setColour(const vec4& colour) { colour_ = colour; }

	// ----------------------------------------------------------------------------

	void uploadData(
		InitVertexArrayFn initVertexArrayFn,
		const size_t vertexSize, 
		const int numVertices, 
		const void* vertexData,
		const int numIndices, 
		const void* indexData)
	{
		rmt_ScopedCPUSample(UploadData);

		initialise(initVertexArrayFn);

		LVN_ASSERT(vao_ > 0);
		LVN_ASSERT(ibuffer_ > 0);
		LVN_ASSERT(vbuffer_ > 0);

		{
			rmt_ScopedCPUSample(BufferData);

			glBindVertexArray(vao_);		

			glBindBuffer(GL_ARRAY_BUFFER, vbuffer_);
			glBufferData(GL_ARRAY_BUFFER, vertexSize * numVertices, vertexData, GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer_);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * numIndices, indexData, GL_STATIC_DRAW);

			numVertices_ = numVertices;
			numTriangles_ = numIndices / 3;

			glBindVertexArray(0);
		}
	}
	
	// ----------------------------------------------------------------------------

	void draw(GLSLProgram& program)
	{
		LVN_ASSERT(vao_ > 0);
		LVN_ASSERT(numTriangles_ > 0);

		glm::mat4 t = glm::translate(glm::mat4(1.f), position_);
		t[0] = vec4(transform_[0], 0.f);
		t[1] = vec4(transform_[1], 0.f);
		t[2] = vec4(transform_[2], 0.f);
		program.setUniform("modelToWorldMatrix", t);
		program.setUniform("u_colour", colour_);

		glBindVertexArray(vao_);
		glDrawElements(GL_TRIANGLES, numTriangles_ * 3, GL_UNSIGNED_INT, (void*)(0));
	}

private:

	// disallow:
	RenderMesh(const RenderMesh&);
	RenderMesh& operator=(const RenderMesh&);

	// ----------------------------------------------------------------------------

	void initialise(InitVertexArrayFn initVertexArray)
	{
		rmt_ScopedCPUSample(MeshInitialise);

		if (vao_ > 0)
		{
			rmt_ScopedCPUSample(Destroy);
			destroy();
		}

		{
			rmt_ScopedCPUSample(Init);
//			rmt_ScopedOpenGLSample(Init);

			{
//				rmt_ScopedCPUSample(Gen);
				glGenBuffers(1, &vbuffer_);
			}

			glGenVertexArrays(1, &vao_);
			glGenBuffers(1, &ibuffer_);

			glBindVertexArray(vao_);
			glBindBuffer(GL_ARRAY_BUFFER, vbuffer_);
			
			initVertexArray();
		}
	}


	// ----------------------------------------------------------------------------

	GLuint      vao_ = 0;
	GLuint      vbuffer_ = 0;
	GLuint      ibuffer_ = 0;
	vec3        position_;
	int	        numVertices_ = 0;
	int         numTriangles_ = 0;
	mat3		transform_{1.f};
	vec4        colour_{1.f};
};

#endif	//	HAS_RENDER_MESH_H_BEEN_INCLUDED
