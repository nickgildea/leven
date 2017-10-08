#if DRAW_MODE == VOXEL_DRAW
layout(location=0) in vec4 position;
layout(location=1) in vec4 normal;
layout(location=2) in vec4 colour;
#elif DRAW_MODE == ACTOR_DRAW
layout(location=0) in vec4 position;
#else
#error No draw mode defined
#endif

uniform mat4 modelToWorldMatrix;
uniform mat4 worldToCameraMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

smooth out vec4 vs_vertexWorldPosition;
smooth out vec4 vs_vertexColour;
smooth out float vs_vertexDepth;

#if DRAW_MODE == VOXEL_DRAW
flat out int vs_vertexMaterial;
smooth out vec4 vs_vertexNormal;
#endif

uniform vec4 u_colour;

#ifdef USE_SHADOWS
out vec4 vs_shadowPosition;
uniform mat4 shadowMVP;
#endif

void main()
{
	vec4 p = vec4(position.xyz, 1.f);
	mat4 modelView = worldToCameraMatrix * modelToWorldMatrix;
	vs_vertexWorldPosition = modelToWorldMatrix * p;

#if DRAW_MODE == VOXEL_DRAW
	vs_vertexColour = vec4(colour.xyz, 0.f);
	vs_vertexMaterial = int(colour.w);
	vs_vertexNormal = normal;
#elif DRAW_MODE == ACTOR_DRAW
	vs_vertexColour = u_colour;
#endif

	vec4 viewspaceP = modelView * p;
	vs_vertexDepth = -viewspaceP.z;
	
#ifdef USE_SHADOWS
	vs_shadowPosition = shadowMVP * p;
#endif

	gl_Position = (projectionMatrix * modelView) * p;
}

