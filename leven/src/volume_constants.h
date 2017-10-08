#ifndef		HAS_VOLUME_CONSTANTS_H_BEEN_INCLUDED
#define		HAS_VOLUME_CONSTANTS_H_BEEN_INCLUDED

#include	<glm/glm.hpp>

// Not sure if I want to use the nodes with size==1 for the min size
const int LEAF_SIZE_LOG2 = 2;
const int LEAF_SIZE_SCALE = 1 << LEAF_SIZE_LOG2;

// 64^3 or 128^3 seems a good size, 256^3 works
// 512^3 and above starts running into problems with buffer sizes etc
// 64^3 seems to be preferable due to clipmap limations (can't display part of an octree) 
// and CSG ops seem more responsive with 64^3 -- worth testing both when upgrading things in future
const int CLIPMAP_VOXELS_PER_CHUNK = 64;
const int CLIPMAP_LEAF_SIZE = LEAF_SIZE_SCALE * CLIPMAP_VOXELS_PER_CHUNK;

// TODO there is an implicit leaf size scaling here that should be handled explicitly
// think that requires removal of LEAF_SIZE_SCALE from the compute_ files (i.e. 
// the compute module should have no knowledge of the sizing, which can be handled 
// separately by the calling code)
const int COLLISION_VOXELS_PER_CHUNK = 128 / 2;
const int COLLISION_NODE_SIZE = CLIPMAP_LEAF_SIZE * (4 / 2);

const glm::ivec3 CHILD_MIN_OFFSETS[] =
{
	// needs to match the vertMap from Dual Contouring impl
	glm::ivec3( 0, 0, 0 ),
	glm::ivec3( 0, 0, 1 ),
	glm::ivec3( 0, 1, 0 ),
	glm::ivec3( 0, 1, 1 ),
	glm::ivec3( 1, 0, 0 ),
	glm::ivec3( 1, 0, 1 ),
	glm::ivec3( 1, 1, 0 ),
	glm::ivec3( 1, 1, 1 ),
};

#endif	//	HAS_VOLUME_CONSTANTS_H_BEEN_INCLUDED
