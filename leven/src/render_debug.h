#ifndef		HAS_RENDER_DEBUG_H_BEEN_INCLUDED
#define		HAS_RENDER_DEBUG_H_BEEN_INCLUDED

#include	"render_types.h"

#include	<vector>
#include	<glm/glm.hpp>

using glm::vec3;

static const vec3 RenderColour_Red(1.f, 0.f, 0.f);
static const vec3 RenderColour_Green(0.f, 1.f, 0.f);
static const vec3 RenderColour_Blue(0.f, 0.f, 1.f);
static const vec3 RenderColour_Black(0.f, 0.f, 0.f);
static const vec3 RenderColour_White(1.f, 1.f, 1.f);

struct RenderDebugCubeInfo
{
	float    min[3];
	float    max[3];
};

struct RenderDebugSphereInfo
{
	float    origin[3];
	float	 radius;
};

struct RenderDebugLineInfo
{
	float    start[3];
	float    end[3];
};

struct RenderDebugCmd
{
	RenderShape                shape;
	vec3                       rgb;
	float		               alpha;

	union
	{
		RenderDebugCubeInfo    cube;
		RenderDebugSphereInfo  sphere;
		RenderDebugLineInfo    line;
	};
};

typedef std::vector<RenderDebugCmd> RenderDebugCmdArray;

class RenderDebugCmdBuffer
{
public:

	RenderDebugCmdBuffer& addCube(const vec3& rgb, const float alpha, const vec3& min, const float size)
	{
		RenderDebugCmd cmd;
		cmd.shape = RenderShape_Cube;
		cmd.rgb = rgb;
		cmd.alpha = alpha;
		cmd.cube.min[0] = min.x;
		cmd.cube.min[1] = min.y;
		cmd.cube.min[2] = min.z;
		cmd.cube.max[0] = min.x + size;
		cmd.cube.max[1] = min.y + size;
		cmd.cube.max[2] = min.z + size;
		cmds_.push_back(cmd);

		return *this;
	}

	RenderDebugCmdBuffer& addSphere(const vec3& rgb, const float alpha, const vec3& origin, const float radius)
	{
		RenderDebugCmd cmd;
		cmd.shape = RenderShape_Sphere;
		cmd.rgb = rgb;
		cmd.alpha = alpha;
		cmd.sphere.origin[0] = origin.x;
		cmd.sphere.origin[1] = origin.y;
		cmd.sphere.origin[2] = origin.z;
		cmd.sphere.radius = radius;
		cmds_.push_back(cmd);

		return *this;
	}

	RenderDebugCmdBuffer& addLine(const vec3& rgb, const float alpha, const vec3& start, const vec3& end)
	{
		RenderDebugCmd cmd;
		cmd.shape = RenderShape_Line;
		cmd.rgb = rgb;
		cmd.alpha = alpha;
		cmd.line.start[0] = start.x;
		cmd.line.start[1] = start.y;
		cmd.line.start[2] = start.z;
		cmd.line.end[0] = end.x;
		cmd.line.end[1] = end.y;
		cmd.line.end[2] = end.z;
		cmds_.push_back(cmd);

		return *this;
	}

	bool empty() const
	{
		return cmds_.empty();
	}

	const RenderDebugCmdArray& commands() const
	{
		return cmds_;
	}

private:

	RenderDebugCmdArray		cmds_;
};

int Render_AllocDebugDrawBuffer();
int Render_FreeDebugDrawBuffer(int* id);
int Render_SetDebugDrawCmds(const int id, const RenderDebugCmdBuffer& cmds);
int Render_EnableDebugDrawBuffer(int id);
int Render_DisableDebugDrawBuffer(int id);

#endif	//	HAS_RENDER_DEBUG_H_BEEN_INCLUDED

