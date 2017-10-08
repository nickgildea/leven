#ifndef		HAS_MODEL_H_BEEN_INCLUDED
#define		HAS_MODEL_H_BEEN_INCLUDED

#include	<glm/glm.hpp>
#include	<sstream>
#include	<vector>
#include	<string>
#include	<map>

#include	"render_types.h"

struct ObjModel
{
	struct Vertex 
	{ 
		glm::vec4				position;
		glm::vec4				normal;
		glm::vec4				colour;			// not used
	};

	struct Triangle 
	{ 
		int indices[3]; 
	};

	std::vector<Vertex>			vertices;
	std::vector<Triangle>		triangles;
};

ObjModel LoadModelFromFile(const std::string& path);

#endif	//	HAS_MODEL_H_BEEN_INCLUDED

