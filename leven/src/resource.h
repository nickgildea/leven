#ifndef		HAS_RESOURCE_H_BEEN_INCLUDED
#define		HAS_RESOURCE_H_BEEN_INCLUDED

#include	"model_obj.h"
#include	"sdl_wrapper.h"

GLuint		Resource_LoadTexture(const char* path);
GLuint		Resource_LoadTextureArray(const std::vector<std::string>& textureFiles);

#endif	//	HAS_RESOURCE_H_BEEN_INCLUDED
