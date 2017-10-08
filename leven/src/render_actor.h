#ifndef		HAS_RENDER_ACTOR_H_BEEN_INCLUDED
#define		HAS_RENDER_ACTOR_H_BEEN_INCLUDED

#include	"render_types.h"

ActorMeshBuffer* Render_CreateActorMesh(const RenderShape shape, const float size);
void Render_ReleaseActorMeshBuffer(ActorMeshBuffer* buffer);

#endif	//	HAS_RENDER_ACTOR_H_BEEN_INCLUDED
