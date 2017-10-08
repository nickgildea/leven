layout (binding=0) uniform sampler2DArray TextureArray;

in vec4 gs_shadowPosition;
layout (binding=1) uniform sampler2DShadow shadowMap;

#if DRAW_MODE == VOXEL_DRAW
layout (binding=2) uniform samplerBuffer MaterialLookup;
#endif

smooth in vec4 gs_position;
smooth in vec4 gs_colour;
smooth in float gs_depth;

#if DRAW_MODE == VOXEL_DRAW
smooth in vec4 gs_normal;
flat in int	gs_material;
#endif

noperspective in vec3 gs_edgeDistance;

uniform int u_useTextures;
uniform vec3 u_cameraPosition;
uniform int u_showWireframe;

layout (location=1) out vec3 NormalData;
layout (location=2) out vec3 ColourData;
layout (location=3) out float LinearDepthData;

uniform float enableLight;
uniform int showNormals;
uniform vec3 lightDir;

uniform mat4 projectionMatrix;
uniform float textureScale;

#if DRAW_MODE == VOXEL_DRAW
vec3 GetBlendedTextureColour(vec3 texturePos, vec3 normal)
{
	float slope = max(0.f, 1.f - normal.y);

	float grass = 0.f;
	float rock = 1.f;

	if (slope < 0.1)
	{
		rock = 0.f;
		grass = 1.f;
	}

	vec3 blendWeights = abs(normal);
	blendWeights = (blendWeights - 0.2) * 7;
	blendWeights = max(blendWeights, 0);
	blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z).xxx;

	int offset = gs_material * 3;
	float idX = texelFetch(MaterialLookup, offset + 0).r;
	float idY = texelFetch(MaterialLookup, offset + 1).r;
	float idZ = texelFetch(MaterialLookup, offset + 2).r;

	vec3 colour0 = texture(TextureArray, vec3(texturePos.yz, idX)).rgb;
	vec3 colour1 = (rock * texture(TextureArray, vec3(texturePos.xz, idX)).rgb) + 
				   (grass * texture(TextureArray, vec3(texturePos.xz, idY)).rgb);
	vec3 colour2 = texture(TextureArray, vec3(texturePos.xy, idZ)).rgb;

	vec3 blendColour = 
		colour0 * blendWeights.xxx +
		colour1 * blendWeights.yyy +
		colour2 * blendWeights.zzz;
	
	return blendColour;
}

// trick from GPU Pro 1: use a different scale for the texture depending on the 
// distance from the camera which helps reduce repeating patterns on far away geom
vec3 colourGeometry(vec3 position, vec3 normal)
{
	const float voxelDistanceNear = 50.f;
	const float voxelDistanceFar = 2000.f;

	const float voxelScaleNear = 32.f;
	const float voxelScaleFar = 128.f;

	const float distanceToCamera = length(u_cameraPosition - position);
	const float voxelDistance = clamp(
		(distanceToCamera - voxelDistanceNear) / (voxelDistanceFar - voxelDistanceNear), 0.f, 1.f);

	const vec3 voxelTexturePosNear = position * (1.f / voxelScaleNear);
	const vec3 voxelTexturePosFar = position * (1.f / voxelScaleFar);

	const vec3 colourNear = GetBlendedTextureColour(voxelTexturePosNear, normal);
	const vec3 colourFar = GetBlendedTextureColour(voxelTexturePosFar, normal);

	return mix(colourNear, colourFar, voxelDistance);
}
#endif

float calculateWireframeValue()
{
	const float d = min(gs_edgeDistance.x, min(gs_edgeDistance.y, gs_edgeDistance.z));
	const float lineWidth = 0.25f;

	float mixValue = 0.f;
	if (d < (lineWidth - 1.f))
	{
		mixValue = 1.f;
	}
	else if (d > (lineWidth + 1.f))
	{
		mixValue = 0.f;
	}
	else
	{
		const float x = d - (lineWidth - 1.f);
		mixValue = exp2(-2.f * x * x);
	}

	return mixValue;
}

void main()
{
	float shadow = textureProj(shadowMap, gs_shadowPosition) < 1.f ? 0.3f : 1.0f;

	// TODO get rid of gs_normal altogether?
	vec3 dFdxPos = dFdx(gs_position.xyz);
	vec3 dFdyPos = dFdy(gs_position.xyz);
	vec3 normal = normalize(cross(dFdxPos, dFdyPos));

#if DRAW_MODE == VOXEL_DRAW

	/*
	if (length(gs_normal) > 0)
	{
		normal = mix(normal, vec3(gs_normal), 0.5);
	}
	*/

	if (u_useTextures > 0)
	{
		ColourData = colourGeometry(gs_position.xyz, normal) * shadow;
	}
	else
	{
		ColourData = gs_colour.xyz;
	}

#elif DRAW_MODE == ACTOR_DRAW
	ColourData = gs_colour.xyz * shadow;
#endif

	if (u_showWireframe > 0)
	{
		const float wireframeValue = calculateWireframeValue();
		ColourData = mix(ColourData, vec3(1.f), wireframeValue);
	}

	NormalData = vec3(0.5) + (0.5 * normal);
	LinearDepthData = gs_depth / FRUSTUM_FAR;
}

